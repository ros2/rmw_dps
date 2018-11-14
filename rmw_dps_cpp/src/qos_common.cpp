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

#include "qos_common.hpp"

const char * qos_history_string(rmw_qos_history_policy_t history)
{
  switch (history) {
    case RMW_QOS_POLICY_HISTORY_KEEP_LAST:
      return "KEEP_LAST";
    case RMW_QOS_POLICY_HISTORY_KEEP_ALL:
      return "KEEP_ALL";
    case RMW_QOS_POLICY_HISTORY_SYSTEM_DEFAULT:
      return "SYSTEM_DEFAULT";
    default:
      return nullptr;
  }
}

const char * qos_reliability_string(rmw_qos_reliability_policy_t reliability)
{
  switch (reliability) {
    case RMW_QOS_POLICY_RELIABILITY_BEST_EFFORT:
      return "BEST_EFFORT";
    case RMW_QOS_POLICY_RELIABILITY_RELIABLE:
      return "RELIABLE";
    case RMW_QOS_POLICY_RELIABILITY_SYSTEM_DEFAULT:
      return "SYSTEM_DEFAULT";
    default:
      return nullptr;
  }
}

const char * qos_durability_string(rmw_qos_durability_policy_t durability)
{
  switch (durability) {
    case RMW_QOS_POLICY_DURABILITY_TRANSIENT_LOCAL:
      return "TRANSIENT_LOCAL";
    case RMW_QOS_POLICY_DURABILITY_VOLATILE:
      return "VOLATILE";
    case RMW_QOS_POLICY_DURABILITY_SYSTEM_DEFAULT:
      return "SYSTEM_DEFAULT";
    default:
      return nullptr;
  }
}
