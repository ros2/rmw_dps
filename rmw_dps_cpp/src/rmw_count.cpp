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

#include "rmw/rmw.h"

extern "C"
{
rmw_ret_t
rmw_count_publishers(
  const rmw_node_t * node,
  const char * topic_name,
  size_t * count)
{
  RCUTILS_LOG_DEBUG_NAMED(
    "rmw_dps_cpp",
    "%s(node=%p,topic_name=%s,count=%p)", __FUNCTION__, node, topic_name, count)

  // TODO
  *count = 0;
  return RMW_RET_OK;
}

rmw_ret_t
rmw_count_subscribers(
  const rmw_node_t * node,
  const char * topic_name,
  size_t * count)
{
  RCUTILS_LOG_DEBUG_NAMED(
    "rmw_dps_cpp",
    "%s(node=%p,topic_name=%s,count=%p)", __FUNCTION__, node, topic_name, count)

  // TODO
  *count = 0;
  return RMW_RET_OK;
}
}  // extern "C"
