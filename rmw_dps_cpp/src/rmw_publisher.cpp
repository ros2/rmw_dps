// Copyright 2018 Intel Corporation All rights reserved.
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

#include <string>

#include "rcutils/logging_macros.h"

#include "rmw/allocators.h"
#include "rmw/error_handling.h"
#include "rmw/rmw.h"

#include "rmw_dps_cpp/custom_node_info.hpp"
#include "rmw_dps_cpp/custom_publisher_info.hpp"
#include "rmw_dps_cpp/identifier.hpp"
#include "type_support_common.hpp"

const char * qos_history_string(rmw_qos_history_policy_t history)
{
  switch (history) {
    case RMW_QOS_POLICY_HISTORY_KEEP_LAST:
      return "KEEP_LAST";
    case RMW_QOS_POLICY_HISTORY_KEEP_ALL:
      return "KEEP_ALL";
    case RMW_QOS_POLICY_HISTORY_SYSTEM_DEFAULT:
      return "SYSTEM_DEFAULT";
    default:
      return nullptr;
  }
}

const char * qos_reliability_string(rmw_qos_reliability_policy_t reliability)
{
  switch (reliability) {
    case RMW_QOS_POLICY_RELIABILITY_BEST_EFFORT:
      return "BEST_EFFORT";
    case RMW_QOS_POLICY_RELIABILITY_RELIABLE:
      return "RELIABLE";
    case RMW_QOS_POLICY_RELIABILITY_SYSTEM_DEFAULT:
      return "SYSTEM_DEFAULT";
    default:
      return nullptr;
  }
}

const char * qos_durability_string(rmw_qos_durability_policy_t durability)
{
  switch (durability) {
    case RMW_QOS_POLICY_DURABILITY_TRANSIENT_LOCAL:
      return "TRANSIENT_LOCAL";
    case RMW_QOS_POLICY_DURABILITY_VOLATILE:
      return "VOLATILE";
    case RMW_QOS_POLICY_DURABILITY_SYSTEM_DEFAULT:
      return "SYSTEM_DEFAULT";
    default:
      return nullptr;
  }
}

extern "C"
{
rmw_publisher_t *
rmw_create_publisher(
  const rmw_node_t * node,
  const rosidl_message_type_support_t * type_supports,
  const char * topic_name,
  const rmw_qos_profile_t * qos_policies)
{
  RCUTILS_LOG_DEBUG_NAMED(
    "rmw_dps_cpp",
    "%s(node=%p,type_supports=%p,topic_name=%s,"
    "qos_policies={history=%s,depth=%d,reliability=%s,durability=%s})",
    __FUNCTION__, node, type_supports, topic_name, qos_history_string(qos_policies->history),
    qos_policies->depth, qos_reliability_string(qos_policies->reliability),
    qos_durability_string(qos_policies->durability));

  if (!node) {
    RMW_SET_ERROR_MSG("node handle is null");
    return nullptr;
  }

  if (node->implementation_identifier != intel_dps_identifier) {
    RMW_SET_ERROR_MSG("node handle not from this implementation");
    return nullptr;
  }

  if (!topic_name || strlen(topic_name) == 0) {
    RMW_SET_ERROR_MSG("publisher topic is null or empty string");
    return nullptr;
  }

  if (!qos_policies) {
    RMW_SET_ERROR_MSG("qos_profile is null");
    return nullptr;
  }

  auto impl = static_cast<CustomNodeInfo *>(node->data);
  if (!impl) {
    RMW_SET_ERROR_MSG("node impl is null");
    return nullptr;
  }

  if (!impl->node_) {
    RMW_SET_ERROR_MSG("node handle is null");
    return nullptr;
  }

  const rosidl_message_type_support_t * type_support = get_message_typesupport_handle(
    type_supports, rosidl_typesupport_introspection_c__identifier);
  if (!type_support) {
    type_support = get_message_typesupport_handle(
      type_supports, rosidl_typesupport_introspection_cpp::typesupport_identifier);
    if (!type_support) {
      RMW_SET_ERROR_MSG("type support not from this implementation");
      return nullptr;
    }
  }

  CustomPublisherInfo * info = nullptr;
  // Topic string cannot start with a separator (/)
  const char * topic = &topic_name[1];
  rmw_publisher_t * rmw_publisher = nullptr;
  DPS_Status ret;

  info = new CustomPublisherInfo();
  info->typesupport_identifier_ = type_support->typesupport_identifier;

  std::string type_name = _create_type_name(
    type_support->data, "msg", info->typesupport_identifier_);
  if (!_get_registered_type(impl->node_, type_name, &info->type_support_)) {
    info->type_support_ = _create_message_type_support(type_support->data,
        info->typesupport_identifier_);
    _register_type(impl->node_, info->type_support_, info->typesupport_identifier_);
  }

  info->publication_ = DPS_CreatePublication(impl->node_);
  if (!info->publication_) {
    RMW_SET_ERROR_MSG("failed to create publication");
    goto fail;
  }
  ret = DPS_InitPublication(info->publication_, &topic, 1, DPS_FALSE, nullptr, nullptr);
  if (ret != DPS_OK) {
    RMW_SET_ERROR_MSG("failed to initialize publication");
    goto fail;
  }

  rmw_publisher = rmw_publisher_allocate();
  if (!rmw_publisher) {
    RMW_SET_ERROR_MSG("failed to allocate publisher");
    goto fail;
  }
  rmw_publisher->implementation_identifier = intel_dps_identifier;
  rmw_publisher->data = info;
  rmw_publisher->topic_name = reinterpret_cast<char *>(
    rmw_allocate(strlen(topic_name) + 1));
  if (!rmw_publisher->topic_name) {
    RMW_SET_ERROR_MSG("failed to allocate memory for publisher topic name");
    goto fail;
  }
  memcpy(const_cast<char *>(rmw_publisher->topic_name), topic_name, strlen(topic_name) + 1);

  // TODO(malsbat): triggering the guard condition to emulate EDP for now
  {
    rmw_ret_t ret = rmw_trigger_guard_condition(impl->graph_guard_condition_);
    if (ret != RMW_RET_OK) {
      RCUTILS_LOG_ERROR_NAMED(
        "rmw_dps_cpp",
        "failed to trigger graph guard condition: %s",
        rmw_get_error_string().str);
    }
  }

  return rmw_publisher;

fail:
  _delete_typesupport(info->type_support_, info->typesupport_identifier_);
  if (info->publication_) {
    DPS_DestroyPublication(info->publication_);
  }
  delete info;

  if (rmw_publisher) {
    if (rmw_publisher->topic_name) {
      rmw_free(const_cast<char *>(rmw_publisher->topic_name));
    }
    rmw_publisher_free(rmw_publisher);
  }

  return nullptr;
}

rmw_ret_t
rmw_publisher_count_matched_subscriptions(
  const rmw_publisher_t * publisher,
  size_t * subscription_count)
{
  // TODO(malsbat): implement
  RCUTILS_LOG_DEBUG_NAMED(
    "rmw_dps_cpp",
    "%s(publisher=%p,subscription_count=%p)", __FUNCTION__, publisher, subscription_count);

  RMW_CHECK_ARGUMENT_FOR_NULL(publisher, RMW_RET_INVALID_ARGUMENT);
  RMW_CHECK_ARGUMENT_FOR_NULL(subscription_count, RMW_RET_INVALID_ARGUMENT);

  return RMW_RET_OK;
}

rmw_ret_t
rmw_destroy_publisher(rmw_node_t * node, rmw_publisher_t * publisher)
{
  RCUTILS_LOG_DEBUG_NAMED(
    "rmw_dps_cpp",
    "%s(node=%p,publisher=%p)", __FUNCTION__, node, publisher);

  if (!node) {
    RMW_SET_ERROR_MSG("node handle is null");
    return RMW_RET_ERROR;
  }

  if (node->implementation_identifier != intel_dps_identifier) {
    RMW_SET_ERROR_MSG("publisher handle not from this implementation");
    return RMW_RET_ERROR;
  }

  if (!publisher) {
    RMW_SET_ERROR_MSG("publisher handle is null");
    return RMW_RET_ERROR;
  }

  if (publisher->implementation_identifier != intel_dps_identifier) {
    RMW_SET_ERROR_MSG("publisher handle not from this implementation");
    return RMW_RET_ERROR;
  }

  auto info = static_cast<CustomPublisherInfo *>(publisher->data);
  if (info) {
    if (info->publication_) {
      DPS_DestroyPublication(info->publication_);
    }
    if (info->type_support_) {
      auto impl = static_cast<CustomNodeInfo *>(node->data);
      if (!impl) {
        RMW_SET_ERROR_MSG("node impl is null");
        return RMW_RET_ERROR;
      }

      _unregister_type(impl->node_, info->type_support_, info->typesupport_identifier_);
    }
  }
  delete info;
  if (publisher->topic_name) {
    rmw_free(const_cast<char *>(publisher->topic_name));
  }
  rmw_publisher_free(publisher);

  return RMW_RET_OK;
}
}  // extern "C"
