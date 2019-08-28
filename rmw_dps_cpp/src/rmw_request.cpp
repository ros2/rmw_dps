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
#include <utility>

#include "rcutils/logging_macros.h"

#include "rmw/error_handling.h"
#include "rmw/rmw.h"

#include "rmw_dps_cpp/CborStream.hpp"
#include "rmw_dps_cpp/custom_client_info.hpp"
#include "rmw_dps_cpp/custom_service_info.hpp"
#include "rmw_dps_cpp/identifier.hpp"
#include "publish_common.hpp"
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
    "%s(client=%p,ros_request=%p,sequence_id=%p)", __FUNCTION__, (void *)client, ros_request,
    (void *)sequence_id);

  assert(client);
  assert(ros_request);
  assert(sequence_id);

  rmw_ret_t returnedValue = RMW_RET_ERROR;

  if (client->implementation_identifier != intel_dps_identifier) {
    RMW_SET_ERROR_MSG("node handle not from this implementation");
    return RMW_RET_ERROR;
  }

  auto info = static_cast<CustomClientInfo *>(client->data);
  assert(info);

  rmw_dps_cpp::cbor::TxStream ser;

  if (_serialize_ros_message(ros_request, ser, info->request_type_support_,
    info->typesupport_identifier_))
  {
    DPS_Status status = publish(info->request_publication_, ser.data(), ser.size());
    if (status == DPS_OK) {
      *sequence_id = DPS_PublicationGetSequenceNum(info->request_publication_);
      returnedValue = RMW_RET_OK;
    } else {
      RMW_SET_ERROR_MSG_WITH_FORMAT_STRING("cannot publish data - %s", DPS_ErrTxt(status));
    }
  } else {
    RMW_SET_ERROR_MSG("cannot serialize data");
  }

  return returnedValue;
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
    "%s(service=%p,request_header=%p,ros_request=%p,taken=%p)", __FUNCTION__, (void *)service,
    (void *)request_header, ros_request, (void *)taken);

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

  rmw_dps_cpp::cbor::RxStream buffer;
  Publication pub;

  if (info->request_listener_->takeNextData(buffer, pub)) {
    _deserialize_ros_message(buffer, ros_request, info->request_type_support_,
      info->typesupport_identifier_);

    // Get header
    memset(request_header->writer_guid, 0, sizeof(request_header->writer_guid));
    const DPS_UUID * uuid = DPS_PublicationGetUUID(pub.get());
    if (uuid) {
      memcpy(request_header->writer_guid, uuid, sizeof(DPS_UUID));
    }
    request_header->sequence_number = DPS_PublicationGetSequenceNum(pub.get());

    *taken = true;

    info->requests_.emplace(std::make_pair(*request_header, std::move(pub)));
  }

  return RMW_RET_OK;
}
}  // extern "C"
