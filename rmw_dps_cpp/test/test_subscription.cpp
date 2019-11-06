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

#include "test_fixtures.hpp"

class test_subscription : public test_fixture_node
{
};

TEST_F(test_subscription, count_matched_publishers) {
  rmw_ret_t ret;
  const rosidl_message_type_support_t * type_support;
  rmw_publisher_t * publisher;
  rmw_subscription_t * subscription;
  size_t count;
  rmw_wait_set_t * wait_set = rmw_create_wait_set(&context, 1);
  rmw_time_t timeout = {1, 0};

  type_support = ROSIDL_GET_MSG_TYPE_SUPPORT(test_msgs, msg, Empty);
  ASSERT_TRUE(nullptr != type_support);

  subscription = rmw_create_subscription(node, type_support, "/count_matched_subscriptions",
      &rmw_qos_profile_default, false);
  ASSERT_TRUE(nullptr != subscription);

  // discover that a matching publisher has been created
  publisher = rmw_create_publisher(node, type_support, "/count_matched_subscriptions",
      &rmw_qos_profile_default);
  ASSERT_TRUE(nullptr != publisher);
  // wait for advertisements to settle down
  ret = rmw_wait(nullptr, nullptr, nullptr, nullptr, nullptr, wait_set, &timeout);
  ASSERT_EQ(RMW_RET_TIMEOUT, ret);
  ret = rmw_subscription_count_matched_publishers(subscription, &count);
  ASSERT_EQ(RMW_RET_OK, ret);
  ASSERT_EQ(1u, count);

  // discover that a matching publisher has been destroyed
  ret = rmw_destroy_publisher(node, publisher);
  ASSERT_EQ(RMW_RET_OK, ret);
  ret = rmw_wait(nullptr, nullptr, nullptr, nullptr, nullptr, wait_set, &timeout);
  ASSERT_EQ(RMW_RET_TIMEOUT, ret);
  ret = rmw_subscription_count_matched_publishers(subscription, &count);
  ASSERT_EQ(RMW_RET_OK, ret);
  ASSERT_EQ(0u, count);

  ret = rmw_destroy_subscription(node, subscription);
  ASSERT_EQ(RMW_RET_OK, ret);
}
