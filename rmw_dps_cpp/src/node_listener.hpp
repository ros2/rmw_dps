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

#ifndef RMW_DPS_CPP__NODE_LISTENER_HPP_
#define RMW_DPS_CPP__NODE_LISTENER_HPP_

#include <dps/dps.h>
#include <dps/Node.hpp>

#include "rmw/rmw.h"

#include "types/guard_condition.hpp"

class NodeListener : public dps::NodeListener
{
public:
  NodeListener(
    rmw_guard_condition_t * graph_guard_condition)
  : graph_guard_condition_(static_cast<GuardCondition *>(graph_guard_condition->data))
  {}
  virtual ~NodeListener() {};
  virtual void onNewChange(dps::Node * node, const dps::RemoteNode * remote)
  {
    (void)node;
    (void)remote;
    graph_guard_condition_->trigger();
  }
  GuardCondition * graph_guard_condition_;
};

#endif  // RMW_DPS_CPP__NODE_LISTENER_HPP_
