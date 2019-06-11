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
#include <rosidl_generator_c/message_type_support_struct.h>
#include <test_msgs/msg/empty.h>

#include <chrono>

#include "gmock/gmock.h"

#include "rmw/node_security_options.h"
#include "rmw/rmw.h"

class test_publisher : public ::testing::Test
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
    node = rmw_create_node(&context, "test_publisher_node", "/", 0, &security_options);
    ASSERT_TRUE(nullptr != node);
  }

  void
  TearDown() override
  {
    rmw_ret_t ret;
    ret = rmw_destroy_node(node);
    ASSERT_EQ(RMW_RET_OK, ret);
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
  rmw_node_t * node;
};

TEST_F(test_publisher, count_matched_subscriptions) {
  rmw_ret_t ret;
  const rosidl_message_type_support_t * type_support;
  rmw_subscription_t * subscription;
  rmw_publisher_t * publisher;
  size_t count;
  rmw_wait_set_t * wait_set = rmw_create_wait_set(&context, 1);
  rmw_time_t timeout = {1, 0};

  type_support = ROSIDL_GET_MSG_TYPE_SUPPORT(test_msgs, msg, Empty);
  ASSERT_TRUE(nullptr != type_support);

  publisher = rmw_create_publisher(node, type_support, "/count_matched_subscriptions",
      &rmw_qos_profile_default);
  ASSERT_TRUE(nullptr != publisher);

  subscription = rmw_create_subscription(node, type_support, "/count_matched_subscriptions",
      &rmw_qos_profile_default, false);
  ASSERT_TRUE(nullptr != subscription);
  // wait for advertisements to settle down
  ret = rmw_wait(nullptr, nullptr, nullptr, nullptr, nullptr, wait_set, &timeout);
  ASSERT_EQ(RMW_RET_TIMEOUT, ret);

  ret = rmw_publisher_count_matched_subscriptions(publisher, &count);
  ASSERT_EQ(RMW_RET_OK, ret);
  ASSERT_EQ(1u, count);

  ret = rmw_destroy_publisher(node, publisher);
  ASSERT_EQ(RMW_RET_OK, ret);
  ret = rmw_destroy_subscription(node, subscription);
  ASSERT_EQ(RMW_RET_OK, ret);
}
