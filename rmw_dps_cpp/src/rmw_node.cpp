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
#include <new>

#include "rcutils/logging_macros.h"

#include "rmw/allocators.h"
#include "rmw/error_handling.h"
#include "rmw/impl/cpp/macros.hpp"
#include "rmw/rmw.h"

#include "rmw_dps_cpp/custom_node_info.hpp"
#include "rmw_dps_cpp/identifier.hpp"

extern "C"
{
void
node_destroyed(DPS_Node * node, void * data)
{
  (void)node;
  DPS_Event * event = reinterpret_cast<DPS_Event *>(data);
  DPS_SignalEvent(event, DPS_OK);
}

rmw_ret_t
destroy_node(DPS_Node * node)
{
  DPS_Event * event = DPS_CreateEvent();
  if (!event) {
    RMW_SET_ERROR_MSG("failed to allocate DPS_Event");
    return RMW_RET_ERROR;
  }
  DPS_Status ret = DPS_DestroyNode(node, node_destroyed, event);
  if (ret == DPS_OK) {
    DPS_WaitForEvent(event);
  }
  DPS_DestroyEvent(event);
  return RMW_RET_OK;
}

rmw_node_t *
create_node(
  rmw_context_t * context,
  const char * name,
  const char * namespace_)
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
  rmw_guard_condition_t * graph_guard_condition = nullptr;
  CustomNodeInfo * node_impl = nullptr;
  rmw_node_t * node_handle = nullptr;
  DPS_Status ret;

  graph_guard_condition = rmw_create_guard_condition(context);
  if (!graph_guard_condition) {
    // error already set
    goto fail;
  }

  try {
    node_impl = new CustomNodeInfo();
  } catch (std::bad_alloc &) {
    RMW_SET_ERROR_MSG("failed to allocate node impl struct");
    goto fail;
  }

  node_handle = rmw_node_allocate();
  if (!node_handle) {
    RMW_SET_ERROR_MSG("failed to allocate rmw_node_t");
    goto fail;
  }
  node_handle->implementation_identifier = intel_dps_identifier;
  node_impl->graph_guard_condition_ = graph_guard_condition;
  node_handle->data = node_impl;

  node_handle->name =
    static_cast<const char *>(rmw_allocate(sizeof(char) * strlen(name) + 1));
  if (!node_handle->name) {
    RMW_SET_ERROR_MSG("failed to allocate memory");
    node_handle->namespace_ = nullptr;  // to avoid free on uninitialized memory
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

  node_impl->node_ = DPS_CreateNode(nullptr, nullptr, nullptr);
  if (!node_impl->node_) {
    RMW_SET_ERROR_MSG("failed to allocate DPS_Node");
    goto fail;
  }
  ret = DPS_StartNode(node_impl->node_, DPS_MCAST_PUB_ENABLE_SEND | DPS_MCAST_PUB_ENABLE_RECV, 0);
  if (ret != DPS_OK) {
    RMW_SET_ERROR_MSG("failed to start DPS_Node");
    goto fail;
  }

  return node_handle;
fail:
  if (node_handle) {
    rmw_free(const_cast<char *>(node_handle->namespace_));
    node_handle->namespace_ = nullptr;
    rmw_free(const_cast<char *>(node_handle->name));
    node_handle->name = nullptr;
  }
  rmw_node_free(node_handle);
  if (node_impl->node_) {
    rmw_ret_t ret = destroy_node(node_impl->node_);
    if (ret != RMW_RET_OK) {
      RCUTILS_LOG_ERROR_NAMED(
        "rmw_dps_cpp",
        "failed to destroy node during error handling");
    }
  }
  delete node_impl;
  if (graph_guard_condition) {
    rmw_ret_t ret = rmw_destroy_guard_condition(graph_guard_condition);
    if (ret != RMW_RET_OK) {
      RCUTILS_LOG_ERROR_NAMED(
        "rmw_dps_cpp",
        "failed to destroy guard condition during error handling");
    }
  }
  return nullptr;
}

rmw_node_t *
rmw_create_node(
  rmw_context_t * context,
  const char * name,
  const char * namespace_,
  size_t domain_id,
  const rmw_node_security_options_t * security_options)
{
  // TODO(malsbat): implement RMW_SECURITY_ENFORCEMENT_PERMISSIVE, ENFORCE
  RCUTILS_LOG_DEBUG_NAMED(
    "rmw_dps_cpp",
    "%s(name=%s,namespace_=%s,domain_id=%lu,"
    "security_options={enforce_security=%d,security_root_path=%s})",
    __FUNCTION__, name, namespace_, domain_id, security_options->enforce_security,
    security_options->security_root_path);

  RCUTILS_CHECK_ARGUMENT_FOR_NULL(context, NULL);
  RMW_CHECK_TYPE_IDENTIFIERS_MATCH(
    init context,
    context->implementation_identifier,
    intel_dps_identifier,
    return NULL);

  if (!name) {
    RMW_SET_ERROR_MSG("name is null");
    return nullptr;
  }
  if (!security_options) {
    RMW_SET_ERROR_MSG("security_options is null");
    return nullptr;
  }

  return create_node(context, name, namespace_);
}

rmw_ret_t
rmw_destroy_node(rmw_node_t * node)
{
  RCUTILS_LOG_DEBUG_NAMED(
    "rmw_dps_cpp",
    "%s(node=%p)", __FUNCTION__, (void*)node);

  rmw_ret_t result_ret = RMW_RET_OK;
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

  rmw_free(const_cast<char *>(node->name));
  node->name = nullptr;
  rmw_free(const_cast<char *>(node->namespace_));
  node->namespace_ = nullptr;
  rmw_node_free(node);

  if (impl->node_) {
    if (RMW_RET_OK != destroy_node(impl->node_)) {
      RMW_SET_ERROR_MSG("failed to destroy node");
      result_ret = RMW_RET_ERROR;
    }
  }

  if (RMW_RET_OK != rmw_destroy_guard_condition(impl->graph_guard_condition_)) {
    RMW_SET_ERROR_MSG("failed to destroy graph guard condition");
    result_ret = RMW_RET_ERROR;
  }

  delete impl;

  return result_ret;
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
    "%s(node=%p)", __FUNCTION__, (void*)node);

  auto impl = static_cast<CustomNodeInfo *>(node->data);
  if (!impl) {
    RMW_SET_ERROR_MSG("node impl is null");
    return nullptr;
  }
  return impl->graph_guard_condition_;
}
}  // extern "C"
