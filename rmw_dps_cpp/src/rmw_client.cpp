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
#include "rmw_dps_cpp/custom_client_info.hpp"
#include "rmw_dps_cpp/identifier.hpp"
#include "rmw_dps_cpp/names_common.hpp"
#include "client_service_common.hpp"
#include "type_support_common.hpp"

extern "C"
{
rmw_client_t *
rmw_create_client(
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
    RMW_SET_ERROR_MSG("client topic is null or empty string");
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

  CustomClientInfo * info = nullptr;
  std::string dps_topic = _get_dps_topic_name(impl->domain_id_, service_name);
  const char * topic = dps_topic.c_str();
  rmw_client_t * rmw_client = nullptr;
  DPS_Status ret;

  info = new CustomClientInfo();
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

  info->event_ = DPS_CreateEvent();
  if (!info->event_) {
    RMW_SET_ERROR_MSG("failed to create event");
    goto fail;
  }
  info->request_publication_ = DPS_CreatePublication(impl->node_);
  if (!info->request_publication_) {
    RMW_SET_ERROR_MSG("failed to create publication");
    goto fail;
  }
  info->response_listener_ = new Listener;
  ret = DPS_SetPublicationData(info->request_publication_, info->response_listener_);
  if (ret != DPS_OK) {
    RMW_SET_ERROR_MSG("failed to set subscription data");
    goto fail;
  }
  ret = DPS_InitPublication(info->request_publication_, &topic, 1, DPS_FALSE, nullptr,
      Listener::onAcknowledgement);
  if (ret != DPS_OK) {
    RMW_SET_ERROR_MSG("failed to initialize publication");
    goto fail;
  }

  rmw_client = rmw_client_allocate();
  if (!rmw_client) {
    RMW_SET_ERROR_MSG("failed to allocate memory for client");
    goto fail;
  }
  rmw_client->implementation_identifier = intel_dps_identifier;
  rmw_client->data = info;
  rmw_client->service_name = reinterpret_cast<const char *>(
    rmw_allocate(strlen(service_name) + 1));
  if (!rmw_client->service_name) {
    RMW_SET_ERROR_MSG("failed to allocate memory for client name");
    goto fail;
  }
  memcpy(const_cast<char *>(rmw_client->service_name), service_name, strlen(service_name) + 1);

  return rmw_client;

fail:
  if (impl) {
    if (info->request_type_support_) {
      _unregister_type(impl->node_, info->request_type_support_, info->typesupport_identifier_);
    }

    if (info->response_type_support_) {
      _unregister_type(impl->node_, info->response_type_support_, info->typesupport_identifier_);
    }
  } else {
    RCUTILS_LOG_ERROR_NAMED(
      "rmw_dps_cpp",
      "leaking type support objects because node impl is null");
  }
  // TODO(malsbat): _delete_typesupport ?
  if (info->request_publication_) {
    DPS_DestroyPublication(info->request_publication_);
  }
  if (info->event_) {
    DPS_DestroyEvent(info->event_);
  }
  delete info->response_listener_;
  delete info;

  if (rmw_client) {
    if (rmw_client->service_name) {
      rmw_free(const_cast<char *>(rmw_client->service_name));
    }
    rmw_client_free(rmw_client);
  }

  return nullptr;
}

rmw_ret_t
rmw_destroy_client(rmw_node_t * node, rmw_client_t * client)
{
  RCUTILS_LOG_DEBUG_NAMED(
    "rmw_dps_cpp",
    "%s(node=%p,client=%p)", __FUNCTION__, (void *)node, (void *)client);

  if (!node) {
    RMW_SET_ERROR_MSG("node handle is null");
    return RMW_RET_ERROR;
  }

  if (node->implementation_identifier != intel_dps_identifier) {
    RMW_SET_ERROR_MSG("client handle not from this implementation");
    return RMW_RET_ERROR;
  }

  if (!client) {
    RMW_SET_ERROR_MSG("client handle is null");
    return RMW_RET_ERROR;
  }

  if (client->implementation_identifier != intel_dps_identifier) {
    RMW_SET_ERROR_MSG("client handle not from this implementation");
    return RMW_RET_ERROR;
  }

  auto info = static_cast<CustomClientInfo *>(client->data);
  if (info) {
    if (info->request_type_support_) {
      _unregister_type(info->node_, info->request_type_support_,
        info->typesupport_identifier_);
    }
    if (info->response_type_support_) {
      _unregister_type(info->node_, info->response_type_support_,
        info->typesupport_identifier_);
    }
    // TODO(malsbat): _delete_typesupport ?
    if (info->request_publication_) {
      DPS_DestroyPublication(info->request_publication_);
    }
    if (info->event_) {
      DPS_DestroyEvent(info->event_);
    }
  }
  delete info;
  if (client->service_name) {
    rmw_free(const_cast<char *>(client->service_name));
  }
  rmw_client_free(client);

  return RMW_RET_OK;
}
}  // extern "C"
