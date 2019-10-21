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

#ifndef RMW_DPS_CPP__CUSTOM_CLIENT_INFO_HPP_
#define RMW_DPS_CPP__CUSTOM_CLIENT_INFO_HPP_

#include <dps/dps.h>
#include <dps/event.h>

#include <string>

#include "rmw/rmw.h"

#include "rmw_dps_cpp/Listener.hpp"

typedef struct CustomClientInfo
{
  void * request_type_support_;
  void * response_type_support_;
  DPS_Publication * request_publication_;
  Listener * response_listener_;
  DPS_Node * node_;
  const char * typesupport_identifier_;
  std::string discovery_name_;
} CustomClientInfo;

#endif  // RMW_DPS_CPP__CUSTOM_CLIENT_INFO_HPP_
