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

#include <dps/CborStream.hpp>

#include "rcutils/logging_macros.h"

#include "rmw/error_handling.h"
#include "rmw/rmw.h"

#include "rmw_dps_cpp/SubscriberListener.hpp"
#include "rmw_dps_cpp/custom_client_info.hpp"
#include "rmw_dps_cpp/custom_service_info.hpp"
#include "rmw_dps_cpp/identifier.hpp"
#include "ros_message_serialization.hpp"

extern "C"
{
rmw_ret_t
rmw_send_request(
  const rmw_client_t * client,
  const void * ros_request,
  int64_t * sequence_id)
{
  RCUTILS_LOG_DEBUG_NAMED(
    "rmw_dps_cpp",
    "%s(client=%p,ros_request=%p,sequence_id=%p)", __FUNCTION__, client, ros_request, sequence_id);

  assert(client);
  assert(ros_request);
  assert(sequence_id);

  if (client->implementation_identifier != intel_dps_identifier) {
    RMW_SET_ERROR_MSG("node handle not from this implementation");
    return RMW_RET_ERROR;
  }

  auto info = static_cast<CustomClientInfo *>(client->data);
  assert(info);

  dps::TxStream ser;
  dps::PublicationInfo serInfo;

  if (!_serialize_ros_message(ros_request, ser, info->request_type_support_,
    info->typesupport_identifier_))
  {
    RMW_SET_ERROR_MSG("cannot serialize data");
    return RMW_RET_ERROR;
  }
  DPS_Status ret = info->publisher_->publish(std::move(ser), &serInfo);
  if (ret != DPS_OK) {
    RMW_SET_ERROR_MSG("cannot publish data");
    return RMW_RET_ERROR;
  }
  *sequence_id = serInfo.sn;
  return RMW_RET_OK;
}

rmw_ret_t
rmw_take_request(
  const rmw_service_t * service,
  rmw_request_id_t * request_header,
  void * ros_request,
  bool * taken)
{
  RCUTILS_LOG_DEBUG_NAMED(
    "rmw_dps_cpp",
    "%s(service=%p,request_header=%p,ros_request=%p,taken=%p)", __FUNCTION__, service, request_header, ros_request, taken);

  assert(service);
  assert(request_header);
  assert(ros_request);
  assert(taken);

  *taken = false;

  if (service->implementation_identifier != intel_dps_identifier) {
    RMW_SET_ERROR_MSG("publisher handle not from this implementation");
    return RMW_RET_ERROR;
  }

  CustomServiceInfo * info = static_cast<CustomServiceInfo *>(service->data);
  assert(info);

  dps::RxStream ser;
  dps::PublicationInfo serInfo;

  if (info->subscriber_->takeNextData(ser, serInfo)) {
    info->listener_->data_taken();
    if (!_deserialize_ros_message(ser, ros_request, info->request_type_support_,
                                  info->typesupport_identifier_)) {
        return RMW_RET_ERROR;
    }
    memcpy(request_header->writer_guid, &serInfo.uuid, sizeof(request_header->writer_guid));
    request_header->sequence_number = serInfo.sn;
    *taken = true;
  }

  return RMW_RET_OK;
}
}  // extern "C"
