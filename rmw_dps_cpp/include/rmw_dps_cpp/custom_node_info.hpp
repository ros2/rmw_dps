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

#include <map>
#include <vector>

#include <dps/dps.h>

#include "rmw/rmw.h"

#include "rmw_dps_cpp/namespace_prefix.hpp"

class NodeListener;

typedef struct CustomNodeInfo
{
  DPS_Node * node_;
  std::string uuid_;
  rmw_guard_condition_t * graph_guard_condition_;
  size_t domain_id_;
  std::vector<std::string> advertisement_topics_;
  DPS_Publication * advertisement_;
  DPS_Subscription * discover_;
  NodeListener * listener_;
} CustomNodeInfo;

inline bool
operator<(const DPS_UUID & lhs, const DPS_UUID & rhs)
{
  return DPS_UUIDCompare(&lhs, &rhs) < 0;
}

class NodeListener
{
public:
  static void
  onPublication(DPS_Subscription * sub, const DPS_Publication * pub, uint8_t * payload, size_t len)
  {
    RCUTILS_LOG_DEBUG_NAMED(
      "rmw_dps_cpp",
      "%s(sub=%p,pub=%p,payload=%p,len=%zu)", __FUNCTION__, (void*)sub, (void*)pub, payload, len);

    NodeListener * listener = reinterpret_cast<NodeListener *>(DPS_GetSubscriptionData(sub));

    std::string namespace_ = "/";
    std::string name;
    for (size_t i = 0; i < DPS_PublicationGetNumTopics(pub); ++i) {
      std::string topic = DPS_PublicationGetTopic(pub, i);
      size_t pos;
      pos = topic.find(dps_namespace_prefix);
      if (pos != std::string::npos) {
        namespace_ = topic.substr(pos + strlen(dps_namespace_prefix));
        continue;
      }
      pos = topic.find(dps_name_prefix);
      if (pos != std::string::npos) {
        name = topic.substr(pos + strlen(dps_name_prefix));
        continue;
      }
    }

    if (!name.empty()) {
      const DPS_UUID * uuid = DPS_PublicationGetUUID(pub);
      listener->discovered_namespaces[*uuid] = namespace_;
      listener->discovered_names[*uuid] = name;
    }
  }

  std::vector<std::string> get_discovered_namespaces() const
  {
    std::vector<std::string> namespaces(discovered_namespaces.size());
    size_t i = 0;
    for (auto it : discovered_namespaces) {
      namespaces[i++] = it.second;
    }
    return namespaces;
  }

  std::vector<std::string> get_discovered_names() const
  {
    std::vector<std::string> names(discovered_names.size());
    size_t i = 0;
    for (auto it : discovered_names) {
      names[i++] = it.second;
    }
    return names;
  }

  std::map<DPS_UUID, std::string> discovered_namespaces;
  std::map<DPS_UUID, std::string> discovered_names;
};

#endif  // RMW_DPS_CPP__CUSTOM_NODE_INFO_HPP_
