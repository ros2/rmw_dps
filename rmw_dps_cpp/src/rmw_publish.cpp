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

#include <cassert>

#include "rcutils/logging_macros.h"

#include "rmw/error_handling.h"
#include "rmw/rmw.h"

#include "rmw_dps_cpp/CborStream.hpp"
#include "rmw_dps_cpp/custom_publisher_info.hpp"
#include "rmw_dps_cpp/identifier.hpp"
#include "ros_message_serialization.hpp"

extern "C"
{
rmw_ret_t
rmw_publish(
  const rmw_publisher_t * publisher,
  const void * ros_message,
  rmw_publisher_allocation_t * allocation)
{
  (void)allocation;
  RCUTILS_LOG_DEBUG_NAMED(
    "rmw_dps_cpp",
    "%s(publisher=%p,ros_message=%p,allocation=%p)",
    __FUNCTION__, (void *)publisher, (void *)ros_message, (void *)allocation);

  rmw_ret_t returnedValue = RMW_RET_ERROR;
  RCUTILS_CHECK_FOR_NULL_WITH_MSG(publisher, "publisher pointer is null", return RMW_RET_ERROR);
  RCUTILS_CHECK_FOR_NULL_WITH_MSG(
    ros_message, "ros_message pointer is null", return RMW_RET_ERROR);

  if (publisher->implementation_identifier != intel_dps_identifier) {
    RMW_SET_ERROR_MSG("publisher handle not from this implementation");
    return RMW_RET_ERROR;
  }

  auto info = static_cast<CustomPublisherInfo *>(publisher->data);
  assert(info);

  rmw_dps_cpp::cbor::TxStream ser;

  if (_serialize_ros_message(ros_message, ser, info->type_support_,
    info->typesupport_identifier_))
  {
    DPS_Status ret = DPS_Publish(info->publication_, ser.data(), ser.size(), 0);
    if (ret == DPS_OK) {
      returnedValue = RMW_RET_OK;
    }
  } else {
    RMW_SET_ERROR_MSG("cannot serialize data");
  }

  return returnedValue;
}

rmw_ret_t
rmw_publish_serialized_message(
  const rmw_publisher_t * publisher,
  const rmw_serialized_message_t * serialized_message,
  rmw_publisher_allocation_t * allocation)
{
  (void)allocation;
  RCUTILS_CHECK_FOR_NULL_WITH_MSG(publisher, "publisher pointer is null", return RMW_RET_ERROR);
  RCUTILS_CHECK_FOR_NULL_WITH_MSG(
    serialized_message, "serialized_message pointer is null", return RMW_RET_ERROR);

  if (publisher->implementation_identifier != intel_dps_identifier) {
    RMW_SET_ERROR_MSG("publisher handle not from this implementation");
    return RMW_RET_ERROR;
  }

  auto info = static_cast<CustomPublisherInfo *>(publisher->data);
  RCUTILS_CHECK_FOR_NULL_WITH_MSG(
    info, "publisher info pointer is null", return RMW_RET_ERROR);

  DPS_Status ret = DPS_Publish(
    info->publication_, reinterpret_cast<uint8_t *>(serialized_message->buffer),
    serialized_message->buffer_length, 0);
  if (ret == DPS_OK) {
    RMW_SET_ERROR_MSG("cannot publish data");
    return RMW_RET_ERROR;
  }

  return RMW_RET_OK;
}
}  // extern "C"
