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
#include <rosidl_generator_c/service_type_support_struct.h>
#include <test_msgs/srv/empty.h>

#include <chrono>

#include "gmock/gmock.h"

#include "rmw/get_service_names_and_types.h"
#include "rmw/node_security_options.h"
#include "rmw/rmw.h"

#include "test_fixtures.hpp"

class test_node : public test_fixture_rmw
{
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

  // detect destruction of node
  ret = rmw_destroy_node(existing_node);
  ASSERT_EQ(RMW_RET_OK, ret);
  ret = rmw_wait(nullptr, nullptr, nullptr, nullptr, nullptr, wait_set, &timeout);
  ASSERT_EQ(RMW_RET_TIMEOUT, ret);
  rcutils_string_array_t node_names = rcutils_get_zero_initialized_string_array();
  rcutils_string_array_t node_namespaces = rcutils_get_zero_initialized_string_array();
  ret = rmw_get_node_names(node, &node_names, &node_namespaces);
  ASSERT_EQ(RMW_RET_OK, ret);
  auto end = node_names.data + node_names.size;
  auto it = std::find_if(node_names.data, end, [](char * data) {
        return !strcmp(data, "existing_node");
      });
  for (size_t i = 0; i < node_names.size; ++i) {
    printf("%s ", node_names.data[i]);
  }
  printf("\n");
  ret = rcutils_string_array_fini(&node_namespaces);
  ASSERT_EQ(RMW_RET_OK, ret);
  ret = rcutils_string_array_fini(&node_names);
  ASSERT_EQ(RMW_RET_OK, ret);
  ASSERT_EQ(it, end);

  ret = rmw_destroy_wait_set(wait_set);
  ASSERT_EQ(RMW_RET_OK, ret);
  ret = rmw_destroy_node(node);
  ASSERT_EQ(RMW_RET_OK, ret);
}

TEST_F(test_node, get_service_names_and_types) {
  rmw_ret_t ret;
  rmw_node_t * node;
  const rosidl_service_type_support_t * type_support;
  rmw_service_t * service;
  rcutils_allocator_t allocator;
  rmw_names_and_types_t names_and_types;
  rmw_wait_set_t * wait_set = rmw_create_wait_set(&context, 1);
  rmw_time_t timeout = {1, 0};

  node = rmw_create_node(&context, "test_node", "/", 0, &security_options);
  ASSERT_TRUE(nullptr != node);

  type_support = ROSIDL_GET_SRV_TYPE_SUPPORT(test_msgs, srv, Empty);
  ASSERT_TRUE(nullptr != type_support);

  service = rmw_create_service(node, type_support, "/test_service",
      &rmw_qos_profile_default);
  ASSERT_TRUE(nullptr != service);
  // wait for advertisements to settle down
  ret = rmw_wait(nullptr, nullptr, nullptr, nullptr, nullptr, wait_set, &timeout);
  ASSERT_EQ(RMW_RET_TIMEOUT, ret);

  allocator = rcutils_get_default_allocator();
  names_and_types = rmw_get_zero_initialized_names_and_types();
  ret = rmw_get_service_names_and_types(node, &allocator, &names_and_types);
  ASSERT_EQ(RMW_RET_OK, ret);
  ASSERT_EQ(1u, names_and_types.names.size);
  ASSERT_STREQ("/test_service", names_and_types.names.data[0]);
  ASSERT_EQ(2u, names_and_types.types[0].size);
  ASSERT_STREQ("test_msgs__srv::dps_::Empty_Request_", names_and_types.types[0].data[0]);
  ASSERT_STREQ("test_msgs__srv::dps_::Empty_Response_", names_and_types.types[0].data[1]);

  ret = rmw_names_and_types_fini(&names_and_types);
  ASSERT_EQ(RMW_RET_OK, ret);
  ret = rmw_destroy_service(node, service);
  ASSERT_EQ(RMW_RET_OK, ret);
  ret = rmw_destroy_node(node);
  ASSERT_EQ(RMW_RET_OK, ret);
}

TEST_F(test_node, service_server_is_available) {
  rmw_ret_t ret;
  rmw_node_t * node;
  const rosidl_service_type_support_t * type_support;
  rmw_client_t * client;
  rmw_service_t * service;
  bool is_available;
  rmw_wait_set_t * wait_set = rmw_create_wait_set(&context, 1);
  rmw_time_t timeout = {1, 0};

  node = rmw_create_node(&context, "test_node", "/", 0, &security_options);
  ASSERT_TRUE(nullptr != node);

  type_support = ROSIDL_GET_SRV_TYPE_SUPPORT(test_msgs, srv, Empty);
  ASSERT_TRUE(nullptr != type_support);

  client = rmw_create_client(node, type_support, "/test_service",
      &rmw_qos_profile_default);
  ASSERT_TRUE(nullptr != client);

  // discover that a matching server is available
  service = rmw_create_service(node, type_support, "/test_service",
      &rmw_qos_profile_default);
  ASSERT_TRUE(nullptr != service);
  // wait for advertisements to settle down
  ret = rmw_wait(nullptr, nullptr, nullptr, nullptr, nullptr, wait_set, &timeout);
  ASSERT_EQ(RMW_RET_TIMEOUT, ret);
  ret = rmw_service_server_is_available(node, client, &is_available);
  ASSERT_EQ(RMW_RET_OK, ret);
  ASSERT_TRUE(is_available);

  // discover that a matching server is not available
  ret = rmw_destroy_service(node, service);
  ASSERT_EQ(RMW_RET_OK, ret);
  ret = rmw_wait(nullptr, nullptr, nullptr, nullptr, nullptr, wait_set, &timeout);
  ASSERT_EQ(RMW_RET_TIMEOUT, ret);
  ret = rmw_service_server_is_available(node, client, &is_available);
  ASSERT_EQ(RMW_RET_OK, ret);
  ASSERT_FALSE(is_available);

  ret = rmw_destroy_client(node, client);
  ASSERT_EQ(RMW_RET_OK, ret);
  ret = rmw_destroy_node(node);
  ASSERT_EQ(RMW_RET_OK, ret);
}
