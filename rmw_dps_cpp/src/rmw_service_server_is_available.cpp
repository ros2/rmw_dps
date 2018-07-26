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

#include "rmw/error_handling.h"
#include "rmw/rmw.h"
#include "rmw/impl/cpp/macros.hpp"

#include "rmw_dps_cpp/identifier.hpp"

extern "C"
{
rmw_ret_t
rmw_service_server_is_available(
  const rmw_node_t * node,
  const rmw_client_t * client,
  bool * is_available)
{
  RCUTILS_LOG_DEBUG_NAMED(
    "rmw_dps_cpp",
    "%s(node=%p,client=%p,is_available=%p)", __FUNCTION__, node, client, is_available)

  if (!node) {
    RMW_SET_ERROR_MSG("node handle is null");
    return RMW_RET_ERROR;
  }

  RMW_CHECK_TYPE_IDENTIFIERS_MATCH(
    node handle,
    node->implementation_identifier, intel_dps_identifier,
    return RMW_RET_ERROR);

  if (!client) {
    RMW_SET_ERROR_MSG("client handle is null");
    return RMW_RET_ERROR;
  }

  if (!is_available) {
    RMW_SET_ERROR_MSG("is_available is null");
    return RMW_RET_ERROR;
  }

  // TODO

  // all conditions met, there is a service server available
  *is_available = true;
  return RMW_RET_OK;
}
}  // extern "C"
