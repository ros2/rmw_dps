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

#include <map>
#include <set>
#include <string>

#include "rcutils/logging_macros.h"
#include "rcutils/strdup.h"

#include "rmw/allocators.h"
#include "rmw/convert_rcutils_ret_to_rmw_ret.h"
#include "rmw/error_handling.h"
#include "rmw/get_node_info_and_types.h"
#include "rmw/get_topic_names_and_types.h"
#include "rmw/names_and_types.h"
#include "rmw/rmw.h"

#include "rmw_dps_cpp/custom_node_info.hpp"
#include "rmw_dps_cpp/identifier.hpp"

/**
 * Validate the input data of node_info_and_types functions.
 *
 * @return RMW_RET_INVALID_ARGUMENT for null input args
 * @return RMW_RET_ERROR if identifier is not the same as the input node
 * @return RMW_RET_OK if all input is valid
 */
static rmw_ret_t
_validate_input(
  const rmw_node_t * node,
  rcutils_allocator_t * allocator,
  bool no_demangle,
  rmw_names_and_types_t * topic_names_and_types)
{
  if (!allocator) {
    RMW_SET_ERROR_MSG("allocator is null");
    return RMW_RET_INVALID_ARGUMENT;
  }
  if (!node) {
    RMW_SET_ERROR_MSG("null node handle");
    return RMW_RET_INVALID_ARGUMENT;
  }

  rmw_ret_t ret = rmw_names_and_types_check_zero(topic_names_and_types);
  if (ret != RMW_RET_OK) {
    return ret;
  }

  if (no_demangle) {
    RMW_SET_ERROR_MSG("no_demangle not implemented");
    return RMW_RET_UNSUPPORTED;
  }

  return RMW_RET_OK;
}

/**
 * Validate the input data of node_info_and_types functions.
 *
 * @return RMW_RET_INVALID_ARGUMENT for null input args
 * @return RMW_RET_ERROR if identifier is not the same as the input node
 * @return RMW_RET_OK if all input is valid
 */
static rmw_ret_t
_validate_input(
  const rmw_node_t * node,
  rcutils_allocator_t * allocator,
  const char * node_name,
  const char * node_namespace,
  bool no_demangle,
  rmw_names_and_types_t * topic_names_and_types)
{
  rmw_ret_t ret = _validate_input(node, allocator, no_demangle, topic_names_and_types);
  if (ret != RMW_RET_OK) {
    return ret;
  }

  if (!node_name) {
    RMW_SET_ERROR_MSG("null node name");
    return RMW_RET_INVALID_ARGUMENT;
  }

  if (!node_namespace) {
    RMW_SET_ERROR_MSG("null node namespace");
    return RMW_RET_INVALID_ARGUMENT;
  }

  return RMW_RET_OK;
}

/**
 * Copy topic data to results
 *
 * @param topics to copy over
 * @param allocator to use
 * @param no_demangle true if demangling will not occur
 * @param topic_names_and_types [out] final rmw result
 * @return RMW_RET_OK if successful
 */
static rmw_ret_t
_copy_data_to_results(
  const std::map<std::string, std::set<std::string>> & topics,
  rcutils_allocator_t * allocator,
  rmw_names_and_types_t * topic_names_and_types)
{
  // Copy data to results handle
  if (!topics.empty()) {
    // Setup string array to store names
    rmw_ret_t rmw_ret = rmw_names_and_types_init(topic_names_and_types, topics.size(), allocator);
    if (rmw_ret != RMW_RET_OK) {
      return rmw_ret;
    }
    // Setup cleanup function, in case of failure below
    auto fail_cleanup = [&topic_names_and_types]() {
        rmw_ret_t rmw_ret = rmw_names_and_types_fini(topic_names_and_types);
        if (rmw_ret != RMW_RET_OK) {
          RCUTILS_LOG_ERROR_NAMED(
            "rmw_dps_cpp",
            "error during report of error: %s", rmw_get_error_string().str);
        }
      };
    // For each topic, store the name, initialize the string array for types, and store all types
    size_t index = 0;
    for (const auto & topic_n_types : topics) {
      // Duplicate and store the topic_name
      char * topic_name = rcutils_strdup(topic_n_types.first.c_str(), *allocator);
      if (!topic_name) {
        RMW_SET_ERROR_MSG("failed to allocate memory for topic name");
        fail_cleanup();
        return RMW_RET_BAD_ALLOC;
      }
      topic_names_and_types->names.data[index] = topic_name;
      // Setup storage for types
      {
        rcutils_ret_t rcutils_ret = rcutils_string_array_init(
          &topic_names_and_types->types[index],
          topic_n_types.second.size(),
          allocator);
        if (rcutils_ret != RCUTILS_RET_OK) {
          RMW_SET_ERROR_MSG(rcutils_get_error_string().str);
          fail_cleanup();
          return rmw_convert_rcutils_ret_to_rmw_ret(rcutils_ret);
        }
      }
      // Duplicate and store each type for the topic
      size_t type_index = 0;
      for (const auto & type : topic_n_types.second) {
        char * type_name = rcutils_strdup(type.c_str(), *allocator);
        if (!type_name) {
          RMW_SET_ERROR_MSG("failed to allocate memory for type name");
          fail_cleanup();
          return RMW_RET_BAD_ALLOC;
        }
        topic_names_and_types->types[index].data[type_index] = type_name;
        ++type_index;
      }        // for each type
      ++index;
    }      // for each topic
  }
  return RMW_RET_OK;
}

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
  RCUTILS_LOG_DEBUG_NAMED(
    "rmw_dps_cpp",
    "%s(node=%p,allocator=%p,node_name=%s,node_namespace=%s,no_demangle=%d,"
    "topic_names_and_types=%p)",
    __FUNCTION__, (void *)node, (void *)allocator, node_name, node_namespace, no_demangle,
    reinterpret_cast<void *>(topic_names_and_types));

  rmw_ret_t valid_input = _validate_input(node, allocator, node_name, node_namespace, no_demangle,
      topic_names_and_types);
  if (valid_input != RMW_RET_OK) {
    return valid_input;
  }

  auto impl = static_cast<CustomNodeInfo *>(node->data);
  auto topics = impl->listener_->get_subscriber_names_and_types_by_node(node_name, node_namespace);
  return _copy_data_to_results(topics, allocator, topic_names_and_types);
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
  RCUTILS_LOG_DEBUG_NAMED(
    "rmw_dps_cpp",
    "%s(node=%p,allocator=%p,node_name=%s,node_namespace=%s,no_demangle=%d,"
    "topic_names_and_types=%p)",
    __FUNCTION__, (void *)node, (void *)allocator, node_name, node_namespace, no_demangle,
    reinterpret_cast<void *>(topic_names_and_types));

  rmw_ret_t valid_input = _validate_input(node, allocator, node_name, node_namespace, no_demangle,
      topic_names_and_types);
  if (valid_input != RMW_RET_OK) {
    return valid_input;
  }

  auto impl = static_cast<CustomNodeInfo *>(node->data);
  auto topics = impl->listener_->get_publisher_names_and_types_by_node(node_name, node_namespace);
  return _copy_data_to_results(topics, allocator, topic_names_and_types);
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
    __FUNCTION__, (void *)node, (void *)allocator, node_name, node_namespace,
    (void *)service_names_and_types);

  return RMW_RET_OK;
}

rmw_ret_t
rmw_get_topic_names_and_types(
  const rmw_node_t * node,
  rcutils_allocator_t * allocator,
  bool no_demangle,
  rmw_names_and_types_t * topic_names_and_types)
{
  RCUTILS_LOG_DEBUG_NAMED(
    "rmw_dps_cpp",
    "%s(node=%p,allocator=%p,no_demangle=%d,topic_names_and_types=%p)",
    __FUNCTION__, (void *)node, (void *)allocator, no_demangle, (void *)topic_names_and_types);

  rmw_ret_t valid_input = _validate_input(node, allocator, no_demangle, topic_names_and_types);
  if (valid_input != RMW_RET_OK) {
    return valid_input;
  }

  auto impl = static_cast<CustomNodeInfo *>(node->data);
  auto topics = impl->listener_->get_topic_names_and_types();
  return _copy_data_to_results(topics, allocator, topic_names_and_types);
}
}  // extern "C"
