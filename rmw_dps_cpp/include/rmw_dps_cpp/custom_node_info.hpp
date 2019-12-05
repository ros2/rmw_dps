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

#include <dps/discovery.h>
#include <dps/dps.h>
#include <algorithm>
#include <iterator>
#include <iostream>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "rmw/rmw.h"

#include "rmw_dps_cpp/CborStream.hpp"
#include "rmw_dps_cpp/custom_publisher_info.hpp"
#include "rmw_dps_cpp/custom_subscriber_info.hpp"
#include "rmw_dps_cpp/names_common.hpp"
#include "rmw_dps_cpp/namespace_prefix.hpp"

class NodeListener;

typedef struct CustomNodeInfo
{
  DPS_Node * node_;
  rmw_guard_condition_t * graph_guard_condition_;
  size_t domain_id_;
  std::mutex discovery_mutex_;
  std::vector<std::string> discovery_payload_;
  DPS_DiscoveryService * discovery_svc_;
  NodeListener * listener_;
  std::mutex publishers_mutex_;
  std::map<std::string, std::set<CustomPublisherInfo *>> publishers_;
  std::mutex subscribers_mutex_;
  std::map<std::string, std::set<CustomSubscriberInfo *>> subscribers_;
} CustomNodeInfo;

rmw_ret_t _add_discovery_topics(CustomNodeInfo * impl, const std::vector<std::string> & topics);
rmw_ret_t _add_discovery_topic(CustomNodeInfo * impl, const std::string & topic);
rmw_ret_t _remove_discovery_topic(CustomNodeInfo * impl, const std::string & topic);

inline bool
operator<(const DPS_UUID & lhs, const DPS_UUID & rhs)
{
  return DPS_UUIDCompare(&lhs, &rhs) < 0;
}

class NodeListener
{
public:
  struct Topic
  {
    std::string topic;
    std::vector<std::string> types;
    explicit Topic(const std::string & topic)
    : topic(topic) {}
    bool operator==(const Topic & that) const
    {
      return this->topic == that.topic &&
             this->types == that.types;
    }
  };
  struct Node
  {
    std::string name;
    std::string namespace_;
    std::vector<Topic> subscribers;
    std::vector<Topic> publishers;
    std::vector<Topic> services;
    std::vector<Topic> clients;
    bool operator==(const Node & that) const
    {
      return this->clients == that.clients &&
             this->services == that.services &&
             this->publishers == that.publishers &&
             this->subscribers == that.subscribers &&
             this->name == that.name &&
             this->namespace_ == that.namespace_;
    }
  };

  explicit NodeListener(const rmw_node_t * node)
  : node_(node)
  {}

  static void
  onDiscovery(
    DPS_DiscoveryService * service, const DPS_Publication * pub, uint8_t * payload,
    size_t len)
  {
    RCUTILS_LOG_DEBUG_NAMED(
      "rmw_dps_cpp",
      "%s(service=%p,pub=%p,payload=%p,len=%zu)", __FUNCTION__, (void *)service, (void *)pub,
      payload, len);

    NodeListener * listener =
      reinterpret_cast<NodeListener *>(DPS_GetDiscoveryServiceData(service));
    return listener->onDiscovery(pub, payload, len);
  }

  void
  onDiscovery(const DPS_Publication * pub, uint8_t * payload, size_t len)
  {
    auto impl = static_cast<CustomNodeInfo *>(node_->data);
    Node node;
    std::string uuid = DPS_UUIDToString(DPS_PublicationGetUUID(pub));
    if (payload && len) {
      rmw_dps_cpp::cbor::RxStream deser(payload, len);
      std::vector<std::string> topics;
      deser >> topics;
      for (size_t i = 0; i < topics.size(); ++i) {
        const std::string & topic = topics[i];
        size_t pos;
        pos = topic.find(dps_namespace_prefix);
        if (pos != std::string::npos) {
          node.namespace_ = topic.substr(pos + strlen(dps_namespace_prefix));
          continue;
        }
        pos = topic.find(dps_name_prefix);
        if (pos != std::string::npos) {
          node.name = topic.substr(pos + strlen(dps_name_prefix));
          continue;
        }
        if (process_topic_info(topic, dps_subscriber_prefix, node.subscribers)) {
          continue;
        }
        if (process_topic_info(topic, dps_publisher_prefix, node.publishers)) {
          continue;
        }
        if (process_topic_info(topic, dps_service_prefix, node.services)) {
          continue;
        }
        if (process_topic_info(topic, dps_client_prefix, node.clients)) {
          continue;
        }
      }
    }
    Node old;
    bool trigger;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      auto it = discovered_nodes_.find(uuid);
      if (it != discovered_nodes_.end()) {
        old = it->second;
      }
      if (payload && len) {
        if (it == discovered_nodes_.end()) {
          discovered_nodes_.insert(std::make_pair(uuid, node));
          trigger = true;
        } else if (it->second == node) {
          trigger = false;
        } else {
          it->second = node;
          trigger = true;
        }
      } else {
        trigger = discovered_nodes_.erase(uuid);
      }
    }
    // Update cached subscriber counts
    if (old.subscribers != node.subscribers) {
      std::vector<std::string> added, removed;
      topic_difference(old.subscribers, node.subscribers, added, removed);

      std::lock_guard<std::mutex> lock(impl->publishers_mutex_);
      for (auto topic : removed) {
        for (auto pub : impl->publishers_[topic]) {
          pub->subscriptions_.erase(uuid);
          pub->subscriptions_matched_count_.store(pub->subscriptions_.size());
        }
      }
      for (auto topic : added) {
        for (auto pub : impl->publishers_[topic]) {
          pub->subscriptions_.insert(uuid);
          pub->subscriptions_matched_count_.store(pub->subscriptions_.size());
        }
      }
    }
    // Update cached publisher counts
    if (old.publishers != node.publishers) {
      std::vector<std::string> added, removed;
      topic_difference(old.publishers, node.publishers, added, removed);

      std::lock_guard<std::mutex> lock(impl->subscribers_mutex_);
      for (auto topic : removed) {
        for (auto sub : impl->subscribers_[topic]) {
          sub->publishers_.erase(uuid);
          sub->publishers_matched_count_.store(sub->publishers_.size());
        }
      }
      for (auto topic : added) {
        for (auto sub : impl->subscribers_[topic]) {
          sub->publishers_.insert(uuid);
          sub->publishers_matched_count_.store(sub->publishers_.size());
        }
      }
    }
    // Notify listener
    if (trigger) {
      if (RMW_RET_OK != rmw_trigger_guard_condition(impl->graph_guard_condition_)) {
        RCUTILS_LOG_ERROR_NAMED(
          "rmw_dps_cpp",
          "failed to trigger guard condition");
      }
    }
  }

  std::vector<Node>
  get_discovered_nodes() const
  {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<Node> nodes(discovered_nodes_.size());
    size_t i = 0;
    for (auto it : discovered_nodes_) {
      nodes[i++] = it.second;
    }
    return nodes;
  }

  size_t
  count_subscribers(const char * topic_name) const
  {
    std::lock_guard<std::mutex> lock(mutex_);
    size_t count = 0;
    for (auto it : discovered_nodes_) {
      count += std::count_if(it.second.subscribers.begin(), it.second.subscribers.end(),
          [topic_name](const Topic & subscriber) {return subscriber.topic == topic_name;});
    }
    return count;
  }

  size_t
  count_publishers(const char * topic_name) const
  {
    std::lock_guard<std::mutex> lock(mutex_);
    size_t count = 0;
    for (auto it : discovered_nodes_) {
      count += std::count_if(it.second.publishers.begin(), it.second.publishers.end(),
          [topic_name](const Topic & publisher) {return publisher.topic == topic_name;});
    }
    return count;
  }

  size_t
  count_services(const char * topic_name) const
  {
    std::lock_guard<std::mutex> lock(mutex_);
    size_t count = 0;
    for (auto it : discovered_nodes_) {
      count += std::count_if(it.second.services.begin(), it.second.services.end(),
          [topic_name](const Topic & service) {return service.topic == topic_name;});
    }
    return count;
  }

  std::map<std::string, std::set<std::string>>
  get_subscriber_names_and_types_by_node(const char * name, const char * namespace_)
  {
    std::map<std::string, std::set<std::string>> names_and_types;
    std::lock_guard<std::mutex> lock(mutex_);
    auto uuid_node_pair = std::find_if(discovered_nodes_.begin(), discovered_nodes_.end(),
        [name, namespace_](const std::pair<std::string, Node> & pair) {
          return pair.second.name == name && pair.second.namespace_ == namespace_;
        });
    if (uuid_node_pair != discovered_nodes_.end()) {
      for (auto it : uuid_node_pair->second.subscribers) {
        names_and_types[it.topic].insert(it.types.begin(), it.types.end());
      }
    }
    return names_and_types;
  }

  std::map<std::string, std::set<std::string>>
  get_publisher_names_and_types_by_node(const char * name, const char * namespace_)
  {
    std::map<std::string, std::set<std::string>> names_and_types;
    std::lock_guard<std::mutex> lock(mutex_);
    auto uuid_node_pair = std::find_if(discovered_nodes_.begin(), discovered_nodes_.end(),
        [name, namespace_](const std::pair<std::string, Node> & pair) {
          return pair.second.name == name && pair.second.namespace_ == namespace_;
        });
    if (uuid_node_pair != discovered_nodes_.end()) {
      for (auto it : uuid_node_pair->second.publishers) {
        names_and_types[it.topic].insert(it.types.begin(), it.types.end());
      }
    }
    return names_and_types;
  }

  std::map<std::string, std::set<std::string>>
  get_service_names_and_types_by_node(const char * name, const char * namespace_)
  {
    std::map<std::string, std::set<std::string>> names_and_types;
    std::lock_guard<std::mutex> lock(mutex_);
    auto uuid_node_pair = std::find_if(discovered_nodes_.begin(), discovered_nodes_.end(),
        [name, namespace_](const std::pair<std::string, Node> & pair) {
          return pair.second.name == name && pair.second.namespace_ == namespace_;
        });
    if (uuid_node_pair != discovered_nodes_.end()) {
      for (auto it : uuid_node_pair->second.services) {
        names_and_types[it.topic].insert(it.types.begin(), it.types.end());
      }
    }
    return names_and_types;
  }

  std::map<std::string, std::set<std::string>>
  get_client_names_and_types_by_node(const char * name, const char * namespace_)
  {
    std::map<std::string, std::set<std::string>> names_and_types;
    std::lock_guard<std::mutex> lock(mutex_);
    auto uuid_node_pair = std::find_if(discovered_nodes_.begin(), discovered_nodes_.end(),
        [name, namespace_](const std::pair<std::string, Node> & pair) {
          return pair.second.name == name && pair.second.namespace_ == namespace_;
        });
    if (uuid_node_pair != discovered_nodes_.end()) {
      for (auto it : uuid_node_pair->second.clients) {
        names_and_types[it.topic].insert(it.types.begin(), it.types.end());
      }
    }
    return names_and_types;
  }

  std::map<std::string, std::set<std::string>>
  get_topic_names_and_types()
  {
    std::map<std::string, std::set<std::string>> names_and_types;
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto uuid_node_pair : discovered_nodes_) {
      for (auto it : uuid_node_pair.second.subscribers) {
        names_and_types[it.topic].insert(it.types.begin(), it.types.end());
      }
      for (auto it : uuid_node_pair.second.publishers) {
        names_and_types[it.topic].insert(it.types.begin(), it.types.end());
      }
    }
    return names_and_types;
  }

  std::map<std::string, std::set<std::string>>
  get_service_names_and_types()
  {
    std::map<std::string, std::set<std::string>> names_and_types;
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto uuid_node_pair : discovered_nodes_) {
      for (auto it : uuid_node_pair.second.services) {
        names_and_types[it.topic].insert(it.types.begin(), it.types.end());
      }
    }
    return names_and_types;
  }

private:
  bool
  process_topic_info(
    const std::string & topic_str, const char * prefix,
    std::vector<Topic> & topics)
  {
    size_t pos = topic_str.find(prefix);
    if (pos != std::string::npos) {
      pos = pos + strlen(prefix);
      size_t end_pos = topic_str.find("&types=");
      if (end_pos != std::string::npos) {
        topics.emplace_back(topic_str.substr(pos, end_pos - pos));
        pos = end_pos + strlen("&types=");
      } else {
        topics.emplace_back(topic_str.substr(pos));
      }
      Topic & topic = topics.back();
      while (pos != std::string::npos) {
        end_pos = topic_str.find(",", pos);
        if (end_pos != std::string::npos) {
          topic.types.emplace_back(topic_str.substr(pos, end_pos - pos));
          pos = end_pos + 1;
        } else {
          topic.types.emplace_back(topic_str.substr(pos));
          pos = end_pos;
        }
      }
      return true;
    }
    return false;
  }

  void
  topic_difference(
    const std::vector<Topic> & old_topics, const std::vector<Topic> & new_topics,
    std::vector<std::string> & added, std::vector<std::string> & removed)
  {
    std::set<std::string> old_names, new_names;
    std::transform(old_topics.begin(), old_topics.end(),
      std::inserter(old_names, old_names.begin()),
      [](const Topic & t) -> std::string {return t.topic;});
    std::transform(new_topics.begin(), new_topics.end(),
      std::inserter(new_names, new_names.begin()),
      [](const Topic & t) -> std::string {return t.topic;});

    std::set_difference(old_names.begin(), old_names.end(), new_names.begin(), new_names.end(),
      std::back_inserter(removed));
    std::set_difference(new_names.begin(), new_names.end(), old_names.begin(), old_names.end(),
      std::back_inserter(added));
  }

  mutable std::mutex mutex_;
  std::map<std::string, Node> discovered_nodes_;
  const rmw_node_t * node_;
};

#endif  // RMW_DPS_CPP__CUSTOM_NODE_INFO_HPP_
