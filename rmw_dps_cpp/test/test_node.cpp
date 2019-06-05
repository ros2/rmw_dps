// Copyright 2019 Intel Corporation All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <rcutils/allocator.h>

#include <chrono>

#include "gmock/gmock.h"

#include "rmw/node_security_options.h"
#include "rmw/rmw.h"

class test_node : public ::testing::Test
{
protected:
  void
  SetUp() override
  {
    rmw_ret_t ret;
    init_options = rmw_get_zero_initialized_init_options();
    ret = rmw_init_options_init(&init_options, rcutils_get_default_allocator());
    ASSERT_EQ(RMW_RET_OK, ret);
    context = rmw_get_zero_initialized_context();
    context.implementation_identifier = rmw_get_implementation_identifier();
    ret = rmw_init(&init_options, &context);
    ASSERT_EQ(RMW_RET_OK, ret);
    security_options = rmw_get_default_node_security_options();
  }

  void
  TearDown() override
  {
    rmw_ret_t ret;
    ret = rmw_shutdown(&context);
    ASSERT_EQ(RMW_RET_OK, ret);
    ret = rmw_context_fini(&context);
    ASSERT_EQ(RMW_RET_OK, ret);
    ret = rmw_init_options_fini(&init_options);
    ASSERT_EQ(RMW_RET_OK, ret);
  }

  rmw_init_options_t init_options;
  rmw_context_t context;
  rmw_node_security_options_t security_options;
};

TEST_F(test_node, create) {
  rmw_ret_t ret;
  rmw_node_t * node;

  node = rmw_create_node(&context, "test_node_create", "/", 0, &security_options);
  ASSERT_TRUE(nullptr != node);
  ret = rmw_destroy_node(node);
  ASSERT_EQ(RMW_RET_OK, ret);

  node = rmw_create_node(&context, "test_subscriber_MultiNested", "/test_time_23_42_58", 97,
      &security_options);
  ASSERT_TRUE(nullptr != node);
  ret = rmw_destroy_node(node);
  ASSERT_EQ(RMW_RET_OK, ret);
}

TEST_F(test_node, discover_existing_node) {
  rmw_ret_t ret;
  rmw_node_t * existing_node;
  rmw_node_t * node;
  rmw_wait_set_t * wait_set = rmw_create_wait_set(&context, 1);
  rmw_time_t timeout = {1, 0};

  existing_node = rmw_create_node(&context, "existing_node", "/", 0, &security_options);
  ASSERT_TRUE(nullptr != existing_node);

  // wait for initial advertisements to settle down
  ret = rmw_wait(nullptr, nullptr, nullptr, nullptr, nullptr, wait_set, &timeout);
  ASSERT_EQ(RMW_RET_TIMEOUT, ret);

  node = rmw_create_node(&context, "discovering_node", "/", 0, &security_options);
  ASSERT_TRUE(nullptr != node);
  void * conditions[] = {rmw_node_get_graph_guard_condition(node)->data};
  rmw_guard_conditions_t guard_conditions = {1, conditions};
  while (true) {
    ret = rmw_wait(nullptr, &guard_conditions, nullptr, nullptr, nullptr, wait_set, &timeout);
    ASSERT_EQ(RMW_RET_OK, ret);
    rcutils_string_array_t node_names = rcutils_get_zero_initialized_string_array();
    rcutils_string_array_t node_namespaces = rcutils_get_zero_initialized_string_array();
    ret = rmw_get_node_names(node, &node_names, &node_namespaces);
    ASSERT_EQ(RMW_RET_OK, ret);
    auto end = node_names.data + node_names.size;
    auto it = std::find_if(node_names.data, end, [](char * data) {
          return !strcmp(data, "existing_node");
        });
    ret = rcutils_string_array_fini(&node_namespaces);
    ASSERT_EQ(RMW_RET_OK, ret);
    ret = rcutils_string_array_fini(&node_names);
    ASSERT_EQ(RMW_RET_OK, ret);
    if (it != end) {
      break;
    }
  }

  ret = rmw_destroy_wait_set(wait_set);
  ASSERT_EQ(RMW_RET_OK, ret);
  ret = rmw_destroy_node(node);
  ASSERT_EQ(RMW_RET_OK, ret);
  ret = rmw_destroy_node(existing_node);
  ASSERT_EQ(RMW_RET_OK, ret);
}
