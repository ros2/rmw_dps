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

#include <dps/event.h>
#include <algorithm>
#include <new>
#include <string>
#include <vector>

#include "rcutils/logging_macros.h"

#include "rmw/allocators.h"
#include "rmw/error_handling.h"
#include "rmw/impl/cpp/macros.hpp"
#include "rmw/rmw.h"

#include "rmw_dps_cpp/custom_node_info.hpp"
#include "rmw_dps_cpp/identifier.hpp"
#include "rmw_dps_cpp/names_common.hpp"

rmw_ret_t
_publish_discovery_payload(CustomNodeInfo * impl)
{
  rmw_dps_cpp::cbor::TxStream ser;
  ser << impl->discovery_payload_;
  if (ser.status() == DPS_ERR_OVERFLOW) {
    ser = rmw_dps_cpp::cbor::TxStream(ser.size_needed());
    ser << impl->discovery_payload_;
  }
  DPS_Status status = DPS_DiscoveryPublish(impl->discovery_svc_, ser.data(), ser.size(),
      NodeListener::onDiscovery);
  if (status == DPS_OK) {
    return RMW_RET_OK;
  } else {
    RMW_SET_ERROR_MSG_WITH_FORMAT_STRING("failed to publish to discovery - %s", DPS_ErrTxt(status));
    return RMW_RET_ERROR;
  }
}

rmw_ret_t
_add_discovery_topics(CustomNodeInfo * impl, const std::vector<std::string> & topics)
{
  std::lock_guard<std::mutex> lock(impl->discovery_mutex_);
  impl->discovery_payload_.insert(impl->discovery_payload_.end(),
    topics.begin(), topics.end());
  return _publish_discovery_payload(impl);
}

rmw_ret_t
_add_discovery_topic(CustomNodeInfo * impl, const std::string & topic)
{
  std::lock_guard<std::mutex> lock(impl->discovery_mutex_);
  impl->discovery_payload_.push_back(topic);
  return _publish_discovery_payload(impl);
}

rmw_ret_t
_remove_discovery_topic(CustomNodeInfo * impl, const std::string & topic)
{
  std::lock_guard<std::mutex> lock(impl->discovery_mutex_);
  auto it = std::find_if(impl->discovery_payload_.begin(), impl->discovery_payload_.end(),
      [&](const std::string & str) {return str == topic;});
  if (it != impl->discovery_payload_.end()) {
    impl->discovery_payload_.erase(it);
    return _publish_discovery_payload(impl);
  } else {
    return RMW_RET_OK;
  }
}

extern "C"
{
void
discovery_svc_destroyed(DPS_DiscoveryService * service, void * data)
{
  (void)service;
  DPS_Event * event = reinterpret_cast<DPS_Event *>(data);
  DPS_SignalEvent(event, DPS_OK);
}

void
node_shutdown(DPS_Node * node, void * data)
{
  (void)node;
  DPS_Event * event = reinterpret_cast<DPS_Event *>(data);
  DPS_SignalEvent(event, DPS_OK);
}

void
node_destroyed(DPS_Node * node, void * data)
{
  (void)node;
  DPS_Event * event = reinterpret_cast<DPS_Event *>(data);
  DPS_SignalEvent(event, DPS_OK);
}

rmw_ret_t
destroy_node(rmw_node_t * node)
{
  if (node) {
    auto impl = static_cast<CustomNodeInfo *>(node->data);
    if (impl) {
      DPS_Event * event = DPS_CreateEvent();
      if (!event) {
        RMW_SET_ERROR_MSG("failed to allocate DPS_Event");
        return RMW_RET_ERROR;
      }
      if (impl->discovery_svc_) {
        DPS_Status ret = DPS_DestroyDiscoveryService(impl->discovery_svc_,
            discovery_svc_destroyed, event);
        if (ret == DPS_OK) {
          DPS_WaitForEvent(event);
        }
      }
      if (impl->node_) {
        DPS_Status ret = DPS_ShutdownNode(impl->node_, node_shutdown, event);
        if (ret == DPS_OK) {
          DPS_WaitForEvent(event);
        }
        ret = DPS_DestroyNode(impl->node_, node_destroyed, event);
        if (ret == DPS_OK) {
          DPS_WaitForEvent(event);
        }
      }
      DPS_DestroyEvent(event);
      if (impl->listener_) {
        delete impl->listener_;
      }
      if (impl->graph_guard_condition_) {
        rmw_ret_t ret = rmw_destroy_guard_condition(impl->graph_guard_condition_);
        if (ret != RMW_RET_OK) {
          RCUTILS_LOG_ERROR_NAMED(
            "rmw_dps_cpp",
            "failed to destroy guard condition");
        }
      }
      delete impl;
    }
    if (node->namespace_) {
      rmw_free(const_cast<char *>(node->namespace_));
    }
    if (node->name) {
      rmw_free(const_cast<char *>(node->name));
    }
    rmw_node_free(node);
  }
  return RMW_RET_OK;
}

rmw_node_t *
create_node(
  rmw_context_t * context,
  const char * name,
  const char * namespace_,
  size_t domain_id)
{
  if (!name) {
    RMW_SET_ERROR_MSG("name is null");
    return nullptr;
  }

  if (!namespace_) {
    RMW_SET_ERROR_MSG("namespace_ is null");
    return nullptr;
  }

  // Declare everything before beginning to create things.
  CustomNodeInfo * node_impl = nullptr;
  rmw_node_t * node_handle = nullptr;
  std::vector<std::string> discovery_topics;
  DPS_Status status;

  node_handle = rmw_node_allocate();
  if (!node_handle) {
    RMW_SET_ERROR_MSG("failed to allocate rmw_node_t");
    goto fail;
  }
  node_handle->implementation_identifier = intel_dps_identifier;

  node_handle->name =
    static_cast<const char *>(rmw_allocate(sizeof(char) * strlen(name) + 1));
  if (!node_handle->name) {
    RMW_SET_ERROR_MSG("failed to allocate memory");
    goto fail;
  }
  memcpy(const_cast<char *>(node_handle->name), name, strlen(name) + 1);
  node_handle->namespace_ =
    static_cast<const char *>(rmw_allocate(sizeof(char) * strlen(namespace_) + 1));
  if (!node_handle->namespace_) {
    RMW_SET_ERROR_MSG("failed to allocate memory");
    goto fail;
  }
  memcpy(const_cast<char *>(node_handle->namespace_), namespace_, strlen(namespace_) + 1);

  try {
    node_impl = new CustomNodeInfo();
  } catch (std::bad_alloc &) {
    RMW_SET_ERROR_MSG("failed to allocate node impl struct");
    goto fail;
  }
  node_handle->data = node_impl;
  node_impl->domain_id_ = domain_id;

  node_impl->graph_guard_condition_ = rmw_create_guard_condition(context);
  if (!node_impl->graph_guard_condition_) {
    // error already set
    goto fail;
  }

  node_impl->listener_ = new NodeListener(node_handle);
  node_impl->node_ = DPS_CreateNode("/=,&", nullptr, nullptr);
  if (!node_impl->node_) {
    RMW_SET_ERROR_MSG("failed to allocate DPS_Node");
    goto fail;
  }
  status = DPS_StartNode(node_impl->node_, DPS_MCAST_PUB_ENABLE_RECV, 0);
  if (status != DPS_OK) {
    RMW_SET_ERROR_MSG("failed to start DPS_Node");
    goto fail;
  }

  node_impl->discovery_svc_ = DPS_CreateDiscoveryService(node_impl->node_, "ROS");
  if (!node_impl->discovery_svc_) {
    RMW_SET_ERROR_MSG("failed to allocate discovery service");
    goto fail;
  }
  status = DPS_SetDiscoveryServiceData(node_impl->discovery_svc_, node_impl->listener_);
  if (status != DPS_OK) {
    RMW_SET_ERROR_MSG("failed to set discovery service data");
    goto fail;
  }

  discovery_topics.push_back(dps_namespace_prefix + std::string(namespace_));
  discovery_topics.push_back(dps_name_prefix + std::string(name));
  if (_add_discovery_topics(node_impl, discovery_topics) != RMW_RET_OK) {
    goto fail;
  }

  return node_handle;
fail:
  rmw_ret_t ret = destroy_node(node_handle);
  if (ret != RMW_RET_OK) {
    RCUTILS_LOG_ERROR_NAMED(
      "rmw_dps_cpp",
      "failed to destroy node during error handling");
  }
  return nullptr;
}

rmw_node_t *
rmw_create_node(
  rmw_context_t * context,
  const char * name,
  const char * namespace_,
  size_t domain_id,
  bool localhost_only)
{
  // TODO(malsbat): implement RMW_SECURITY_ENFORCEMENT_PERMISSIVE, ENFORCE
  // TODO(malsbat): implement localhost_only
  RCUTILS_LOG_DEBUG_NAMED(
    "rmw_dps_cpp",
    "%s(name=%s,namespace_=%s,domain_id=%zu,localhost_only=%d)",
    __FUNCTION__, name, namespace_, domain_id, localhost_only);

  RCUTILS_CHECK_ARGUMENT_FOR_NULL(context, nullptr);
  RMW_CHECK_TYPE_IDENTIFIERS_MATCH(
    init context,
    context->implementation_identifier,
    intel_dps_identifier,
    return nullptr);

  if (!name) {
    RMW_SET_ERROR_MSG("name is null");
    return nullptr;
  }

  return create_node(context, name, namespace_, domain_id);
}

rmw_ret_t
rmw_destroy_node(rmw_node_t * node)
{
  RCUTILS_LOG_DEBUG_NAMED(
    "rmw_dps_cpp",
    "%s(node=%p)", __FUNCTION__, (void *)node);

  if (!node) {
    RMW_SET_ERROR_MSG("node handle is null");
    return RMW_RET_ERROR;
  }

  if (node->implementation_identifier != intel_dps_identifier) {
    RMW_SET_ERROR_MSG("node handle not from this implementation");
    return RMW_RET_ERROR;
  }

  return destroy_node(node);
}

rmw_ret_t
rmw_node_assert_liveliness(const rmw_node_t * node)
{
  (void)node;
  RMW_SET_ERROR_MSG("unimplemented");
  return RMW_RET_ERROR;
}

const rmw_guard_condition_t *
rmw_node_get_graph_guard_condition(const rmw_node_t * node)
{
  RCUTILS_LOG_DEBUG_NAMED(
    "rmw_dps_cpp",
    "%s(node=%p)", __FUNCTION__, (void *)node);

  auto impl = static_cast<CustomNodeInfo *>(node->data);
  if (!impl) {
    RMW_SET_ERROR_MSG("node impl is null");
    return nullptr;
  }
  return impl->graph_guard_condition_;
}
}  // extern "C"
