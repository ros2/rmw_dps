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
#include "rmw/impl/cpp/macros.hpp"

#include "rmw_dps_cpp/custom_node_info.hpp"
#include "rmw_dps_cpp/custom_service_info.hpp"
#include "rmw_dps_cpp/identifier.hpp"
#include "rmw_dps_cpp/names_common.hpp"
#include "client_service_common.hpp"
#include "type_support_common.hpp"

extern "C"
{
rmw_service_t *
rmw_create_service(
  const rmw_node_t * node,
  const rosidl_service_type_support_t * type_supports,
  const char * service_name,
  const rmw_qos_profile_t * qos_policies)
{
  RCUTILS_LOG_DEBUG_NAMED(
    "rmw_dps_cpp",
    "%s(node=%p,type_supports=%p,service_name=%s,"
    "qos_policies={history=%d,depth=%zu,reliability=%d,durability=%d})",
    __FUNCTION__, (void *)node, (void *)type_supports, service_name, qos_policies->history,
    qos_policies->depth, qos_policies->reliability, qos_policies->durability);

  if (!node) {
    RMW_SET_ERROR_MSG("node handle is null");
    return nullptr;
  }

  if (node->implementation_identifier != intel_dps_identifier) {
    RMW_SET_ERROR_MSG("node handle not from this implementation");
    return nullptr;
  }

  if (!service_name || strlen(service_name) == 0) {
    RMW_SET_ERROR_MSG("service topic is null or empty string");
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

  const rosidl_service_type_support_t * type_support = get_service_typesupport_handle(
    type_supports, rosidl_typesupport_introspection_c__identifier);
  if (!type_support) {
    type_support = get_service_typesupport_handle(
      type_supports, rosidl_typesupport_introspection_cpp::typesupport_identifier);
    if (!type_support) {
      RMW_SET_ERROR_MSG("type support not from this implementation");
      return nullptr;
    }
  }

  CustomServiceInfo * info = nullptr;
  std::string dps_topic = _get_dps_topic_name(impl->domain_id_, service_name);
  const char * topic = dps_topic.c_str();
  rmw_service_t * rmw_service = nullptr;
  rmw_dps_cpp::cbor::TxStream ser;
  DPS_Status ret;

  info = new CustomServiceInfo();
  info->node_ = impl->node_;
  info->typesupport_identifier_ = type_support->typesupport_identifier;

  const void * untyped_request_members;
  const void * untyped_response_members;

  untyped_request_members =
    get_request_ptr(type_support->data, info->typesupport_identifier_);
  untyped_response_members = get_response_ptr(type_support->data,
      info->typesupport_identifier_);

  std::string request_type_name = _create_type_name(untyped_request_members,
      info->typesupport_identifier_);
  std::string response_type_name = _create_type_name(untyped_response_members,
      info->typesupport_identifier_);

  if (!_get_registered_type(impl->node_, request_type_name, &info->request_type_support_)) {
    info->request_type_support_ = _create_request_type_support(type_support->data,
        info->typesupport_identifier_);
    _register_type(impl->node_, info->request_type_support_, info->typesupport_identifier_);
  }

  if (!_get_registered_type(impl->node_, response_type_name, &info->response_type_support_)) {
    info->response_type_support_ = _create_response_type_support(type_support->data,
        info->typesupport_identifier_);
    _register_type(impl->node_, info->response_type_support_, info->typesupport_identifier_);
  }

  info->request_listener_ = new Listener;
  info->request_subscription_ = DPS_CreateSubscription(impl->node_, &topic, 1);
  if (!info->request_subscription_) {
    RMW_SET_ERROR_MSG("failed to create subscription");
    goto fail;
  }
  ret = DPS_SetSubscriptionData(info->request_subscription_, info->request_listener_);
  if (ret != DPS_OK) {
    RMW_SET_ERROR_MSG("failed to set subscription data");
    goto fail;
  }
  ret = DPS_Subscribe(info->request_subscription_, Listener::onPublication);
  if (ret != DPS_OK) {
    RMW_SET_ERROR_MSG("failed to subscribe");
    goto fail;
  }

  rmw_service = rmw_service_allocate();
  if (!rmw_service) {
    RMW_SET_ERROR_MSG("failed to allocate memory for service");
    goto fail;
  }

  rmw_service->implementation_identifier = intel_dps_identifier;
  rmw_service->data = info;
  rmw_service->service_name = reinterpret_cast<const char *>(
    rmw_allocate(strlen(service_name) + 1));
  if (!rmw_service->service_name) {
    RMW_SET_ERROR_MSG("failed to allocate memory for service name");
    goto fail;
  }
  memcpy(const_cast<char *>(rmw_service->service_name), service_name,
    strlen(service_name) + 1);

  info->discovery_name_ = dps_service_prefix + std::string(service_name) +
    "&types=" + request_type_name + "," + response_type_name;
  impl->discovery_payload_.push_back(info->discovery_name_);
  ser << impl->discovery_payload_;
  ret = DPS_DiscoveryPublish(impl->discovery_svc_, ser.data(), ser.size(),
      NodeListener::onDiscovery);
  if (ret != DPS_OK) {
    RMW_SET_ERROR_MSG("failed to publish to discovery");
    goto fail;
  }

  return rmw_service;

fail:
  if (info->request_subscription_) {
    DPS_DestroySubscription(info->request_subscription_, [](DPS_Subscription * sub) {
        delete reinterpret_cast<Listener *>(DPS_GetSubscriptionData(sub));
      });
  }
  if (info->request_type_support_) {
    _unregister_type(impl->node_, info->request_type_support_, info->typesupport_identifier_);
  }
  if (info->response_type_support_) {
    _unregister_type(impl->node_, info->response_type_support_, info->typesupport_identifier_);
  }
  delete info;

  if (rmw_service) {
    if (rmw_service->service_name) {
      rmw_free(const_cast<char *>(rmw_service->service_name));
    }
    rmw_service_free(rmw_service);
  }

  return nullptr;
}

rmw_ret_t
rmw_destroy_service(rmw_node_t * node, rmw_service_t * service)
{
  RCUTILS_LOG_DEBUG_NAMED(
    "rmw_dps_cpp",
    "%s(node=%p,service=%p)", __FUNCTION__, (void *)node, (void *)service);

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

  if (!service) {
    RMW_SET_ERROR_MSG("service handle is null");
    return RMW_RET_ERROR;
  }
  if (service->implementation_identifier != intel_dps_identifier) {
    RMW_SET_ERROR_MSG("service handle not from this implementation");
    return RMW_RET_ERROR;
  }

  rmw_dps_cpp::cbor::TxStream ser;
  auto info = static_cast<CustomServiceInfo *>(service->data);
  if (info) {
    auto it = std::find_if(impl->discovery_payload_.begin(), impl->discovery_payload_.end(),
        [&](const std::string & str) {return str == info->discovery_name_;});
    if (it != impl->discovery_payload_.end()) {
      impl->discovery_payload_.erase(it);
      ser << impl->discovery_payload_;
      DPS_Status ret = DPS_DiscoveryPublish(impl->discovery_svc_, ser.data(), ser.size(),
          NodeListener::onDiscovery);
      if (ret != DPS_OK) {
        RMW_SET_ERROR_MSG("failed to publish to discovery");
        return RMW_RET_ERROR;
      }
    }
    if (info->request_subscription_) {
      DPS_DestroySubscription(info->request_subscription_, [](DPS_Subscription * sub) {
          delete reinterpret_cast<Listener *>(DPS_GetSubscriptionData(sub));
        });
    }
    if (info->request_type_support_) {
      _unregister_type(info->node_, info->request_type_support_,
        info->typesupport_identifier_);
    }
    if (info->response_type_support_) {
      _unregister_type(info->node_, info->response_type_support_,
        info->typesupport_identifier_);
    }
  }
  delete info;

  if (service->service_name) {
    rmw_free(const_cast<char *>(service->service_name));
  }
  rmw_service_free(service);

  return RMW_RET_OK;
}
}  // extern "C"
