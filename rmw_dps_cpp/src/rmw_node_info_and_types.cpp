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

#include "rcutils/logging_macros.h"

#include "rmw/allocators.h"
#include "rmw/convert_rcutils_ret_to_rmw_ret.h"
#include "rmw/error_handling.h"
#include "rmw/get_node_info_and_types.h"
#include "rmw/names_and_types.h"
#include "rmw/rmw.h"

#include "rmw_dps_cpp/identifier.hpp"

extern "C"
{
rmw_ret_t
rmw_get_subscriber_names_and_types_by_node(
  const rmw_node_t * node,
  rcutils_allocator_t * allocator,
  const char * node_name,
  const char * node_namespace,
  bool no_demangle,
  rmw_names_and_types_t * topic_names_and_types)
{
  // TODO(malsbat): implement
  RCUTILS_LOG_DEBUG_NAMED(
    "rmw_dps_cpp",
    "%s(node=%p,allocator=%p,node_name=%s,node_namespace=%s,no_demangle=%d,"
    "topic_names_and_types=%p)",
    __FUNCTION__, (void*)node, (void*)allocator, node_name, node_namespace, no_demangle,
    (void*)topic_names_and_types);

  return RMW_RET_OK;
}

rmw_ret_t
rmw_get_publisher_names_and_types_by_node(
  const rmw_node_t * node,
  rcutils_allocator_t * allocator,
  const char * node_name,
  const char * node_namespace,
  bool no_demangle,
  rmw_names_and_types_t * topic_names_and_types)
{
  // TODO(malsbat): implement
  RCUTILS_LOG_DEBUG_NAMED(
    "rmw_dps_cpp",
    "%s(node=%p,allocator=%p,node_name=%s,node_namespace=%s,no_demangle=%d,"
    "topic_names_and_types=%p)",
    __FUNCTION__, (void*)node, (void*)allocator, node_name, node_namespace, no_demangle,
    (void*)topic_names_and_types);

  return RMW_RET_OK;
}

rmw_ret_t
rmw_get_service_names_and_types_by_node(
  const rmw_node_t * node,
  rcutils_allocator_t * allocator,
  const char * node_name,
  const char * node_namespace,
  rmw_names_and_types_t * service_names_and_types)
{
  // TODO(malsbat): implement
  RCUTILS_LOG_DEBUG_NAMED(
    "rmw_dps_cpp",
    "%s(node=%p,allocator=%p,node_name=%s,node_namespace=%s,service_names_and_types=%p)",
    __FUNCTION__, (void*)node, (void*)allocator, node_name, node_namespace,
    (void*)service_names_and_types);

  return RMW_RET_OK;
}
}  // extern "C"
