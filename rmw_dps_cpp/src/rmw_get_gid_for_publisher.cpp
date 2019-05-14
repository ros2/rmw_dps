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

#include "rmw_dps_cpp/custom_publisher_info.hpp"
#include "rmw_dps_cpp/identifier.hpp"

extern "C"
{
rmw_ret_t
rmw_get_gid_for_publisher(const rmw_publisher_t * publisher, rmw_gid_t * gid)
{
  RCUTILS_LOG_DEBUG_NAMED(
    "rmw_dps_cpp",
    "%s(publisher=%p,gid=%p)", __FUNCTION__, (void*)publisher, (void*)gid);

  if (!publisher) {
    RMW_SET_ERROR_MSG("publisher is null");
    return RMW_RET_ERROR;
  }

  if (publisher->implementation_identifier != intel_dps_identifier) {
    RMW_SET_ERROR_MSG("publisher handle not from this implementation");
    return RMW_RET_ERROR;
  }

  if (!gid) {
    RMW_SET_ERROR_MSG("gid is null");
    return RMW_RET_ERROR;
  }

  auto info = static_cast<const CustomPublisherInfo *>(publisher->data);
  if (!info || !info->publication_) {
    RMW_SET_ERROR_MSG("publisher info handle is null");
    return RMW_RET_ERROR;
  }

  gid->implementation_identifier = intel_dps_identifier;
  static_assert(
    sizeof(DPS_UUID) <= RMW_GID_STORAGE_SIZE,
    "RMW_GID_STORAGE_SIZE insufficient to store the rmw_dps_cpp GID implementation."
  );

  memset(gid->data, 0, RMW_GID_STORAGE_SIZE);
  const DPS_UUID * uuid = DPS_PublicationGetUUID(info->publication_);
  if (!uuid) {
    RMW_SET_ERROR_MSG("no guid found for publisher");
    return RMW_RET_ERROR;
  }
  memcpy(gid->data, uuid, sizeof(DPS_UUID));
  return RMW_RET_OK;
}
}  // extern "C"
