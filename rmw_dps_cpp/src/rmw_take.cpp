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
#include "rmw/serialized_message.h"
#include "rmw/rmw.h"
#include "rmw/event.h"

#include "rmw_dps_cpp/CborStream.hpp"
#include "rmw_dps_cpp/custom_subscriber_info.hpp"
#include "rmw_dps_cpp/identifier.hpp"
#include "ros_message_serialization.hpp"

extern "C"
{
void
_assign_message_info(
  rmw_message_info_t * message_info,
  const DPS_Publication * pub)
{
  rmw_gid_t * sender_gid = &message_info->publisher_gid;
  sender_gid->implementation_identifier = intel_dps_identifier;
  memset(sender_gid->data, 0, RMW_GID_STORAGE_SIZE);
  const DPS_UUID * uuid = DPS_PublicationGetUUID(pub);
  if (uuid) {
    memcpy(sender_gid->data, uuid, sizeof(DPS_UUID));
  }
}

rmw_ret_t
_take(
  const rmw_subscription_t * subscription,
  void * ros_message,
  bool * taken,
  rmw_message_info_t * message_info)
{
  *taken = false;

  if (subscription->implementation_identifier != intel_dps_identifier) {
    RMW_SET_ERROR_MSG("publisher handle not from this implementation");
    return RMW_RET_ERROR;
  }

  CustomSubscriberInfo * info = static_cast<CustomSubscriberInfo *>(subscription->data);
  RCUTILS_CHECK_FOR_NULL_WITH_MSG(info, "custom subscriber info is null", return RMW_RET_ERROR);

  rmw_dps_cpp::cbor::RxStream buffer;
  Publication pub;

  if (info->listener_->takeNextData(buffer, pub)) {
    _deserialize_ros_message(buffer, ros_message, info->type_support_,
      info->typesupport_identifier_);
    if (message_info) {
      _assign_message_info(message_info, pub.get());
    }
    *taken = true;
  }

  return RMW_RET_OK;
}

rmw_ret_t
rmw_take(
  const rmw_subscription_t * subscription,
  void * ros_message,
  bool * taken,
  rmw_subscription_allocation_t * allocation)
{
  (void)allocation;

  RCUTILS_LOG_DEBUG_NAMED(
    "rmw_dps_cpp",
    "%s(subscription=%p,ros_message=%p,taken=%p,allocation=%p)", __FUNCTION__, (void *)subscription,
    ros_message, (void *)taken, (void *)allocation);

  RCUTILS_CHECK_FOR_NULL_WITH_MSG(
    subscription, "subscription pointer is null", return RMW_RET_ERROR);
  RCUTILS_CHECK_FOR_NULL_WITH_MSG(
    ros_message, "ros_message pointer is null", return RMW_RET_ERROR);
  RCUTILS_CHECK_FOR_NULL_WITH_MSG(
    taken, "boolean flag for taken is null", return RMW_RET_ERROR);

  return _take(subscription, ros_message, taken, nullptr);
}

rmw_ret_t
rmw_take_with_info(
  const rmw_subscription_t * subscription,
  void * ros_message,
  bool * taken,
  rmw_message_info_t * message_info,
  rmw_subscription_allocation_t * allocation)
{
  (void)allocation;

  RCUTILS_LOG_DEBUG_NAMED(
    "rmw_dps_cpp",
    "%s(subscription=%p,ros_message=%p,taken=%p,message_info=%p,allocation=%p)", __FUNCTION__,
    (void *)subscription, ros_message, (void *)taken, (void *)message_info, (void *)allocation);

  RCUTILS_CHECK_FOR_NULL_WITH_MSG(
    subscription, "subscription pointer is null", return RMW_RET_ERROR);
  RCUTILS_CHECK_FOR_NULL_WITH_MSG(
    ros_message, "ros_message pointer is null", return RMW_RET_ERROR);
  RCUTILS_CHECK_FOR_NULL_WITH_MSG(
    taken, "boolean flag for taken is null", return RMW_RET_ERROR);
  RCUTILS_CHECK_FOR_NULL_WITH_MSG(
    message_info, "message info pointer is null", return RMW_RET_ERROR);

  return _take(subscription, ros_message, taken, message_info);
}

rmw_ret_t
_take_serialized_message(
  const rmw_subscription_t * subscription,
  rmw_serialized_message_t * serialized_message,
  bool * taken,
  rmw_message_info_t * message_info)
{
  *taken = false;

  if (subscription->implementation_identifier != intel_dps_identifier) {
    RMW_SET_ERROR_MSG("publisher handle not from this implementation");
    return RMW_RET_ERROR;
  }

  CustomSubscriberInfo * info = static_cast<CustomSubscriberInfo *>(subscription->data);
  RCUTILS_CHECK_FOR_NULL_WITH_MSG(info, "custom subscriber info is null", return RMW_RET_ERROR);

  rmw_dps_cpp::cbor::RxStream buffer;
  Publication pub;

  if (info->listener_->takeNextData(buffer, pub)) {
    auto buffer_size = static_cast<size_t>(buffer.getBufferSize());
    if (serialized_message->buffer_capacity < buffer_size) {
      auto ret = rmw_serialized_message_resize(serialized_message, buffer_size);
      if (ret != RMW_RET_OK) {
        return ret;  // Error message already set
      }
    }
    serialized_message->buffer_length = buffer_size;
    memcpy(serialized_message->buffer, buffer.getBuffer(), serialized_message->buffer_length);

    if (message_info) {
      _assign_message_info(message_info, pub.get());
    }
    *taken = true;
  }

  return RMW_RET_OK;
}

rmw_ret_t
rmw_take_serialized_message(
  const rmw_subscription_t * subscription,
  rmw_serialized_message_t * serialized_message,
  bool * taken,
  rmw_subscription_allocation_t * allocation)
{
  (void)allocation;

  RCUTILS_LOG_DEBUG_NAMED(
    "rmw_dps_cpp",
    "%s(subscription=%p,serialized_message=%p,taken=%p,allocation=%p)", __FUNCTION__,
    (void *)subscription, (void *)serialized_message, (void *)taken, (void *)allocation);

  RCUTILS_CHECK_FOR_NULL_WITH_MSG(
    subscription, "subscription pointer is null", return RMW_RET_ERROR);
  RCUTILS_CHECK_FOR_NULL_WITH_MSG(
    serialized_message, "ros_message pointer is null", return RMW_RET_ERROR);
  RCUTILS_CHECK_FOR_NULL_WITH_MSG(
    taken, "boolean flag for taken is null", return RMW_RET_ERROR);

  return _take_serialized_message(subscription, serialized_message, taken, nullptr);
}

rmw_ret_t
rmw_take_serialized_message_with_info(
  const rmw_subscription_t * subscription,
  rmw_serialized_message_t * serialized_message,
  bool * taken,
  rmw_message_info_t * message_info,
  rmw_subscription_allocation_t * allocation)
{
  (void)allocation;

  RCUTILS_LOG_DEBUG_NAMED(
    "rmw_dps_cpp",
    "%s(subscription=%p,serialized_message=%p,taken=%p,message_info=%p,allocation=%p)",
    __FUNCTION__,
    reinterpret_cast<const void *>(subscription), reinterpret_cast<void *>(serialized_message),
    reinterpret_cast<void *>(taken), reinterpret_cast<void *>(message_info),
    reinterpret_cast<void *>(allocation));

  RCUTILS_CHECK_FOR_NULL_WITH_MSG(
    subscription, "subscription pointer is null", return RMW_RET_ERROR);
  RCUTILS_CHECK_FOR_NULL_WITH_MSG(
    serialized_message, "ros_message pointer is null", return RMW_RET_ERROR);
  RCUTILS_CHECK_FOR_NULL_WITH_MSG(
    taken, "boolean flag for taken is null", return RMW_RET_ERROR);
  RCUTILS_CHECK_FOR_NULL_WITH_MSG(
    message_info, "message info pointer is null", return RMW_RET_ERROR);

  return _take_serialized_message(subscription, serialized_message, taken, message_info);
}

rmw_ret_t
rmw_take_event(
  const rmw_event_t * event_handle,
  void * event_info,
  bool * taken)
{
  (void)event_handle;
  (void)event_info;
  (void)taken;
  RMW_SET_ERROR_MSG("unimplemented");
  return RMW_RET_ERROR;
}
}  // extern "C"
