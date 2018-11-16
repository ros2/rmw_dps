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

#ifndef RMW_DPS_CPP__CUSTOM_NODE_INFO_HPP_
#define RMW_DPS_CPP__CUSTOM_NODE_INFO_HPP_

#include <dps/dps.h>
#include <dps/Node.hpp>

#include "rmw/rmw.h"

class NodeListener;

typedef struct CustomNodeInfo
{
  size_t domain_id_;
  dps::Node * node_;
  NodeListener * listener_;
  rmw_guard_condition_t * graph_guard_condition_;
} CustomNodeInfo;

#endif  // RMW_DPS_CPP__CUSTOM_NODE_INFO_HPP_
