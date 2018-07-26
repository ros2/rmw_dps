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
#include "rmw_dps_cpp/custom_client_info.hpp"
#include "rmw_dps_cpp/custom_service_info.hpp"
#include "rmw_dps_cpp/identifier.hpp"
#include "ros_message_serialization.hpp"

extern "C"
{
rmw_ret_t
rmw_take_response(
  const rmw_client_t * client,
  rmw_request_id_t * request_header,
  void * ros_response,
  bool * taken)
{
  RCUTILS_LOG_DEBUG_NAMED(
    "rmw_dps_cpp",
    "%s(client=%p,request_header=%p,ros_request=%p,taken=%p)", __FUNCTION__, client, request_header, ros_response, taken)

  assert(client);
  assert(request_header);
  assert(ros_response);
  assert(taken);

  *taken = false;

  if (client->implementation_identifier != intel_dps_identifier) {
    RMW_SET_ERROR_MSG("publisher handle not from this implementation");
    return RMW_RET_ERROR;
  }

  CustomClientInfo * info = static_cast<CustomClientInfo *>(client->data);
  assert(info);

  rmw_dps_cpp::cbor::RxStream buffer;
  Publication pub;

  if (info->response_listener_->takeNextData(buffer, pub)) {
    _deserialize_ros_message(buffer, ros_response, info->response_type_support_,
      info->typesupport_identifier_);

    // Get header
    memset(request_header->writer_guid, 0, sizeof(request_header->writer_guid));
    const DPS_UUID * uuid = DPS_PublicationGetUUID(pub.get());
    if (uuid) {
      memcpy(request_header->writer_guid, uuid, sizeof(DPS_UUID));
    }
    request_header->sequence_number = DPS_PublicationGetSequenceNum(pub.get());

    *taken = true;
  }

  return RMW_RET_OK;
}

rmw_ret_t
rmw_send_response(
  const rmw_service_t * service,
  rmw_request_id_t * request_header,
  void * ros_response)
{
  RCUTILS_LOG_DEBUG_NAMED(
    "rmw_dps_cpp",
    "%s(service=%p,request_header=%p,ros_response=%p)", __FUNCTION__, service, request_header, ros_response)

  assert(service);
  assert(request_header);
  assert(ros_response);

  rmw_ret_t returnedValue = RMW_RET_ERROR;

  if (service->implementation_identifier != intel_dps_identifier) {
    RMW_SET_ERROR_MSG("service handle not from this implementation");
    return RMW_RET_ERROR;
  }

  auto info = static_cast<CustomServiceInfo *>(service->data);
  assert(info);

  auto request = info->requests_.find(*request_header);
  if (request == info->requests_.end()) {
    RMW_SET_ERROR_MSG("cannot find request");
    return RMW_RET_ERROR;
  }

  Publication pub = std::move(request->second);
  rmw_dps_cpp::cbor::TxStream ser;

  if (_serialize_ros_message(ros_response, ser, info->response_type_support_,
    info->typesupport_identifier_))
  {
    DPS_Status ret = DPS_AckPublication(pub.get(), ser.data(), ser.size());
    if (ret == DPS_OK) {
      returnedValue = RMW_RET_OK;
    } else {
      RMW_SET_ERROR_MSG("cannot send response");
    }
  } else {
    RMW_SET_ERROR_MSG("cannot serialize data");
  }

  info->requests_.erase(request);
  return returnedValue;
}
}  // extern "C"
