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

#include "rmw_dps_cpp/Listener.hpp"
#include "rmw_dps_cpp/custom_node_info.hpp"
#include "rmw_dps_cpp/custom_subscriber_info.hpp"
#include "rmw_dps_cpp/identifier.hpp"
#include "rmw_dps_cpp/names_common.hpp"
#include "qos_common.hpp"
#include "type_support_common.hpp"

extern "C"
{
rmw_ret_t
rmw_init_subscription_allocation(
  const rosidl_message_type_support_t * type_support,
  const rosidl_message_bounds_t * message_bounds,
  rmw_subscription_allocation_t * allocation)
{
  // Unused in current implementation.
  (void) type_support;
  (void) message_bounds;
  (void) allocation;
  RMW_SET_ERROR_MSG("unimplemented");
  return RMW_RET_ERROR;
}

rmw_ret_t
rmw_fini_subscription_allocation(rmw_subscription_allocation_t * allocation)
{
  // Unused in current implementation.
  (void) allocation;
  RMW_SET_ERROR_MSG("unimplemented");
  return RMW_RET_ERROR;
}

rmw_subscription_t *
rmw_create_subscription(
  const rmw_node_t * node,
  const rosidl_message_type_support_t * type_supports,
  const char * topic_name,
  const rmw_qos_profile_t * qos_policies,
  const rmw_subscription_options_t * subscription_options)
{
  RCUTILS_LOG_DEBUG_NAMED(
    "rmw_dps_cpp",
    "%s(node=%p,type_supports=%p,topic_name=%s,"
    "qos_policies={history=%s,depth=%zu,reliability=%s,durability=%s},subscription_options=%p)",
    __FUNCTION__, reinterpret_cast<const void *>(node),
    reinterpret_cast<const void *>(type_supports), topic_name,
    qos_history_string(qos_policies->history), qos_policies->depth,
    qos_reliability_string(qos_policies->reliability),
    qos_durability_string(qos_policies->durability),
    reinterpret_cast<const void *>(subscription_options));

  if (!node) {
    RMW_SET_ERROR_MSG("node handle is null");
    return nullptr;
  }

  if (node->implementation_identifier != intel_dps_identifier) {
    RMW_SET_ERROR_MSG("node handle not from this implementation");
    return nullptr;
  }

  if (!topic_name || strlen(topic_name) == 0) {
    RMW_SET_ERROR_MSG("subscription topic is null or empty string");
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

  CustomSubscriberInfo * info = nullptr;
  std::string dps_topic = _get_dps_topic_name(impl->domain_id_, topic_name);
  const char * topic = dps_topic.c_str();
  rmw_subscription_t * rmw_subscription = nullptr;
  rmw_dps_cpp::cbor::TxStream ser;
  DPS_Status ret;

  info = new CustomSubscriberInfo();
  info->node_ = node;
  info->typesupport_identifier_ = type_support->typesupport_identifier;

  std::string type_name = _create_type_name(
    type_support->data, info->typesupport_identifier_);
  if (!_get_registered_type(impl->node_, type_name, &info->type_support_)) {
    info->type_support_ = _create_message_type_support(type_support->data,
        info->typesupport_identifier_);
    _register_type(impl->node_, info->type_support_, info->typesupport_identifier_);
  }


  info->qos_ = *qos_policies;
  /* Set to best-effort & volatile since QoS features are not supported by DPS at the moment. */
  info->qos_.history = RMW_QOS_POLICY_HISTORY_KEEP_LAST;
  info->qos_.durability = RMW_QOS_POLICY_DURABILITY_VOLATILE;
  info->qos_.reliability = RMW_QOS_POLICY_RELIABILITY_BEST_EFFORT;

  info->subscription_ = DPS_CreateSubscription(impl->node_, &topic, 1);
  if (!info->subscription_) {
    RMW_SET_ERROR_MSG("failed to create subscription");
    goto fail;
  }
  info->listener_ = new Listener;
  ret = DPS_SetSubscriptionData(info->subscription_, info->listener_);
  if (ret != DPS_OK) {
    RMW_SET_ERROR_MSG("failed to set subscription data");
    goto fail;
  }
  ret = DPS_Subscribe(info->subscription_, Listener::onPublication);
  if (ret != DPS_OK) {
    RMW_SET_ERROR_MSG("failed to subscribe");
    goto fail;
  }

  rmw_subscription = rmw_subscription_allocate();
  if (!rmw_subscription) {
    RMW_SET_ERROR_MSG("failed to allocate subscription");
    goto fail;
  }
  rmw_subscription->implementation_identifier = intel_dps_identifier;
  rmw_subscription->data = info;
  rmw_subscription->topic_name =
    reinterpret_cast<const char *>(rmw_allocate(strlen(topic_name) + 1));
  if (!rmw_subscription->topic_name) {
    RMW_SET_ERROR_MSG("failed to allocate memory for subscription topic name");
    goto fail;
  }
  memcpy(const_cast<char *>(rmw_subscription->topic_name), topic_name,
    strlen(topic_name) + 1);

  info->discovery_name_ = dps_subscriber_prefix + std::string(topic_name) +
    "&types=" + type_name;
  if (_add_discovery_topic(impl, info->discovery_name_) != RMW_RET_OK) {
    goto fail;
  }

  {
    std::lock_guard<std::mutex> lock(impl->subscribers_mutex_);
    impl->subscribers_[topic_name].insert(info);
  }
  return rmw_subscription;

fail:
  if (info->subscription_) {
    DPS_DestroySubscription(info->subscription_, [](DPS_Subscription * sub) {
        delete reinterpret_cast<Listener *>(DPS_GetSubscriptionData(sub));
      });
  }
  if (info->type_support_) {
    _delete_typesupport(info->type_support_, info->typesupport_identifier_);
  }
  delete info;

  if (rmw_subscription) {
    if (rmw_subscription->topic_name) {
      rmw_free(const_cast<char *>(rmw_subscription->topic_name));
    }
    rmw_subscription_free(rmw_subscription);
  }

  return nullptr;
}

rmw_ret_t
rmw_subscription_count_matched_publishers(
  const rmw_subscription_t * subscription,
  size_t * publisher_count)
{
  RCUTILS_LOG_DEBUG_NAMED(
    "rmw_dps_cpp",
    "%s(subscription=%p,publisher_count=%p)", __FUNCTION__,
    (void *)subscription, (void *)publisher_count);

  auto info = static_cast<CustomSubscriberInfo *>(subscription->data);
  if (info != nullptr) {
    *publisher_count = info->publishers_matched_count_.load();
  }
  return RMW_RET_OK;
}

rmw_ret_t
rmw_destroy_subscription(rmw_node_t * node, rmw_subscription_t * subscription)
{
  RCUTILS_LOG_DEBUG_NAMED(
    "rmw_dps_cpp",
    "%s(node=%p,subscription=%p)", __FUNCTION__, (void *)node, (void *)subscription);

  if (!node) {
    RMW_SET_ERROR_MSG("node handle is null");
    return RMW_RET_ERROR;
  }
  if (node->implementation_identifier != intel_dps_identifier) {
    RMW_SET_ERROR_MSG("node handle not from this implementation");
    return RMW_RET_ERROR;
  }
  auto impl = static_cast<CustomNodeInfo *>(node->data);
  if (!impl) {
    RMW_SET_ERROR_MSG("node impl is null");
    return RMW_RET_ERROR;
  }

  if (!subscription) {
    RMW_SET_ERROR_MSG("subscription handle is null");
    return RMW_RET_ERROR;
  }
  if (subscription->implementation_identifier != intel_dps_identifier) {
    RMW_SET_ERROR_MSG("node handle not from this implementation");
    return RMW_RET_ERROR;
  }

  auto info = static_cast<CustomSubscriberInfo *>(subscription->data);
  if (info) {
    if (subscription->topic_name) {
      std::lock_guard<std::mutex> lock(impl->subscribers_mutex_);
      impl->subscribers_[subscription->topic_name].erase(info);
    }
    _remove_discovery_topic(impl, info->discovery_name_);
    if (info->subscription_) {
      DPS_DestroySubscription(info->subscription_, [](DPS_Subscription * sub) {
          delete reinterpret_cast<Listener *>(DPS_GetSubscriptionData(sub));
        });
    }
    if (info->type_support_) {
      _unregister_type(impl->node_, info->type_support_, info->typesupport_identifier_);
    }
  }
  delete info;

  if (subscription->topic_name) {
    rmw_free(const_cast<char *>(subscription->topic_name));
  }
  rmw_subscription_free(subscription);

  return RMW_RET_OK;
}

rmw_ret_t
rmw_subscription_get_actual_qos(
  const rmw_subscription_t * subscription,
  rmw_qos_profile_t * qos)
{
  RMW_CHECK_ARGUMENT_FOR_NULL(subscription, RMW_RET_INVALID_ARGUMENT);
  RMW_CHECK_ARGUMENT_FOR_NULL(qos, RMW_RET_INVALID_ARGUMENT);

  auto info = static_cast<CustomSubscriberInfo *>(subscription->data);
  *qos = info->qos_;
  return RMW_RET_OK;
}
}  // extern "C"
