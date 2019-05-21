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

#include "rmw/error_handling.h"
#include "rmw/serialized_message.h"
#include "rmw/rmw.h"

#include "type_support_common.hpp"
#include "ros_message_serialization.hpp"

extern "C"
{
rmw_ret_t
rmw_serialize(
  const void * ros_message,
  const rosidl_message_type_support_t * type_support,
  rmw_serialized_message_t * serialized_message)
{
  RCUTILS_LOG_DEBUG_NAMED(
    "rmw_dps_cpp",
    "%s(ros_message=%p,type_support=%p,serialized_message=%p)", __FUNCTION__, ros_message,
    (void*)type_support, (void*)serialized_message);

  const rosidl_message_type_support_t * ts = get_message_typesupport_handle(
    type_support, rosidl_typesupport_introspection_c__identifier);
  if (!ts) {
    ts = get_message_typesupport_handle(
      type_support, rosidl_typesupport_introspection_cpp::typesupport_identifier);
    if (!ts) {
      RMW_SET_ERROR_MSG("type support not from this implementation");
      return RMW_RET_ERROR;
    }
  }

  auto tss = _create_message_type_support(ts->data, ts->typesupport_identifier);
  rmw_dps_cpp::cbor::TxStream ser;

  auto ret = _serialize_ros_message(ros_message, ser, tss, ts->typesupport_identifier);
  auto data_length = static_cast<size_t>(ser.size());
  if (serialized_message->buffer_capacity < data_length) {
    if (rmw_serialized_message_resize(serialized_message, data_length) != RMW_RET_OK) {
      RMW_SET_ERROR_MSG("unable to dynamically resize serialized message");
      return RMW_RET_ERROR;
    }
  }
  memcpy(serialized_message->buffer, ser.data(), data_length);
  serialized_message->buffer_length = data_length;
  serialized_message->buffer_capacity = data_length;
  _delete_typesupport(tss, ts->typesupport_identifier);
  return ret == true ? RMW_RET_OK : RMW_RET_ERROR;
}

rmw_ret_t
rmw_deserialize(
  const rmw_serialized_message_t * serialized_message,
  const rosidl_message_type_support_t * type_support,
  void * ros_message)
{
  RCUTILS_LOG_DEBUG_NAMED(
    "rmw_dps_cpp",
    "%s(serialized_message=%p,type_support=%p,ros_message=%p)", __FUNCTION__,
    (void*)serialized_message, (void*)type_support, ros_message);

  const rosidl_message_type_support_t * ts = get_message_typesupport_handle(
    type_support, rosidl_typesupport_introspection_c__identifier);
  if (!ts) {
    ts = get_message_typesupport_handle(
      type_support, rosidl_typesupport_introspection_cpp::typesupport_identifier);
    if (!ts) {
      RMW_SET_ERROR_MSG("type support not from this implementation");
      return RMW_RET_ERROR;
    }
  }

  auto tss = _create_message_type_support(ts->data, ts->typesupport_identifier);
  rmw_dps_cpp::cbor::RxStream buffer(
    (const uint8_t *)serialized_message->buffer, serialized_message->buffer_length);

  auto ret = _deserialize_ros_message(buffer, ros_message, tss, ts->typesupport_identifier);
  _delete_typesupport(tss, ts->typesupport_identifier);
  return ret == true ? RMW_RET_OK : RMW_RET_ERROR;
}

rmw_ret_t
rmw_get_serialized_message_size(
  const rosidl_message_type_support_t * /*type_support*/,
  const rosidl_message_bounds_t * /*message_bounds*/,
  size_t * /*size*/)
{
  RMW_SET_ERROR_MSG("unimplemented");
  return RMW_RET_ERROR;
}
}  // extern "C"
