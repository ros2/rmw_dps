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

#ifndef QOS_COMMON_HPP_
#define QOS_COMMON_HPP_

#include "rmw/rmw.h"

const char * qos_history_string(rmw_qos_history_policy_t history);

const char * qos_reliability_string(rmw_qos_reliability_policy_t reliability);

const char * qos_durability_string(rmw_qos_durability_policy_t durability);

#endif  // QOS_COMMON_HPP_
