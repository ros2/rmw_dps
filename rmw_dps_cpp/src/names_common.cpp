// Copyright 2019 Intel Corporation All rights reserved.
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

#include <dps/event.h>
#include <algorithm>
#include <string>
#include <vector>

#include "rcutils/logging_macros.h"

#include "rmw/error_handling.h"

#include "rmw_dps_cpp/custom_node_info.hpp"
#include "rmw_dps_cpp/names_common.hpp"

std::string
_get_dps_topic_name(size_t domain_id, const char * ros_topic_name)
{
  // Valid topic names start with a separator so prefix the domain_id to separate nodes
  return std::to_string(domain_id) + ros_topic_name;
}

std::string
_get_dps_topic_name(size_t domain_id, const std::string & ros_topic_name)
{
  // Valid topic names start with a separator so prefix the domain_id to separate nodes
  return std::to_string(domain_id) + ros_topic_name;
}

bool
_advertise(const rmw_node_t * node, const std::string topic)
{
  DPS_Status ret;

  auto impl = static_cast<CustomNodeInfo *>(node->data);
  std::lock_guard<std::mutex> lock(impl->mutex_);
  std::vector<std::string> & topics = impl->advertisement_topics_;
  if (!impl->advertisement_) {
    topics.push_back(std::to_string(impl->domain_id_) + dps_uuid_prefix + impl->uuid_);
    // DPS does not allow empty topic segments "=/" from namespace_ below, so use &namespace_[1]
    if (node->namespace_[1]) {
      topics.push_back(std::to_string(impl->domain_id_) + dps_namespace_prefix +
        &node->namespace_[1]);
    }
    topics.push_back(std::to_string(impl->domain_id_) + dps_name_prefix + node->name);
  }
  if (!topic.empty()) {
    topics.push_back(topic);
  }
  std::vector<const char *> ctopics;
  std::transform(topics.begin(), topics.end(), std::back_inserter(ctopics),
    [](const std::string & s) -> const char * {return s.c_str();});

  if (impl->advertisement_) {
    ret = DPS_DestroyPublication(impl->advertisement_, nullptr);
    if (ret != DPS_OK) {
      RCUTILS_LOG_ERROR_NAMED(
        "rmw_dps_cpp",
        "failed to destroy existing advertisement");
    }
  }
  impl->advertisement_ = DPS_CreatePublication(impl->node_);
  if (!impl->advertisement_) {
    RMW_SET_ERROR_MSG("failed to create advertisement");
    return false;
  }
  ret = DPS_InitPublication(impl->advertisement_, &ctopics[0], ctopics.size(), DPS_FALSE,
      nullptr, nullptr);
  if (ret != DPS_OK) {
    RMW_SET_ERROR_MSG("failed to initialize advertisement");
    return false;
  }
  // TODO(malsbat): refresh publication before ttl (one hour below) expires
  ret = DPS_Publish(impl->advertisement_, nullptr, 0, 360);
  if (ret != DPS_OK) {
    RMW_SET_ERROR_MSG("failed to advertise");
    return false;
  }
  return true;
}

void
_advertisement_published(
  DPS_Publication * pub,
  const DPS_Buffer * bufs, size_t numBufs,
  DPS_Status status,
  void * data)
{
  (void)pub;
  (void)bufs;
  (void)numBufs;
  DPS_Event * event = reinterpret_cast<DPS_Event *>(data);
  DPS_SignalEvent(event, status);
}

rmw_ret_t
_destroy_advertisement(const rmw_node_t * node)
{
  auto impl = static_cast<CustomNodeInfo *>(node->data);
  std::lock_guard<std::mutex> lock(impl->mutex_);
  if (!impl->advertisement_) {
    return RMW_RET_OK;
  }
  // force the expiration of the advertisement
  // in order to ensure this is delivered to the network use the callback based version of publish
  DPS_Event * event = DPS_CreateEvent();
  if (!event) {
    RMW_SET_ERROR_MSG("failed to allocate DPS_Event");
    return RMW_RET_ERROR;
  }
  DPS_Status ret = DPS_PublishBufs(impl->advertisement_, nullptr, 0, -1, _advertisement_published,
      event);
  if (ret == DPS_OK) {
    DPS_WaitForEvent(event);
  } else {
    // this is non-fatal: the advertisement will expire at its ttl
    RCUTILS_LOG_ERROR_NAMED(
      "rmw_dps_cpp",
      "failed to expire advertisement");
  }
  DPS_DestroyEvent(event);
  ret = DPS_DestroyPublication(impl->advertisement_, nullptr);
  if (ret != DPS_OK) {
    RMW_SET_ERROR_MSG("failed to destroy advertisement");
    return RMW_RET_ERROR;
  }
  impl->advertisement_ = nullptr;
  return RMW_RET_OK;
}
