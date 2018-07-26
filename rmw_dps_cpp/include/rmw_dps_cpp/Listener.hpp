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

#ifndef RMW_DPS_CPP__LISTENER_HPP_
#define RMW_DPS_CPP__LISTENER_HPP_

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <utility>
#include <vector>

#include <dps/dps.h>

#include "rmw_dps_cpp/CborStream.hpp"

struct PublicationDeleter {
  void operator()(DPS_Publication * pub) { DPS_DestroyPublication(pub); }
};

typedef std::unique_ptr<DPS_Publication, PublicationDeleter> Publication;

class Listener
{
public:
  using Data = std::pair<Publication, rmw_dps_cpp::cbor::RxStream>;

  explicit Listener()
  : conditionMutex_(nullptr), conditionVariable_(nullptr)
  {
  }

  static void
  onPublication(DPS_Subscription* sub, const DPS_Publication* pub, uint8_t* payload, size_t len)
  {
    RCUTILS_LOG_DEBUG_NAMED(
      "rmw_dps_cpp",
      "%s(sub=%p,pub=%p,payload=%p,len=%d)", __FUNCTION__, sub, pub, payload, len)
    RCUTILS_LOG_DEBUG_NAMED(
      "rmw_dps_cpp",
      "pub={uuid=%s,sequenceNum=%d}", DPS_UUIDToString(DPS_PublicationGetUUID(pub)), DPS_PublicationGetSequenceNum(pub));

    Listener * listener = (Listener *) DPS_GetSubscriptionData(sub);
    Data data = std::make_pair(Publication(DPS_CopyPublication(pub)), rmw_dps_cpp::cbor::RxStream(payload, len));
    std::lock_guard<std::mutex> lock(listener->internalMutex_);

    if (listener->conditionMutex_) {
      std::unique_lock<std::mutex> clock(*listener->conditionMutex_);
      // the change to data_ needs to be mutually exclusive with rmw_wait()
      // which checks hasData() and decides if wait() needs to be called
      listener->data_.push(std::move(data));
      clock.unlock();
      listener->conditionVariable_->notify_one();
    } else {
      listener->data_.push(std::move(data));
    }
  }

  static void
  onAcknowledgement(DPS_Publication* pub, uint8_t* payload, size_t len)
  {
    RCUTILS_LOG_DEBUG_NAMED(
      "rmw_dps_cpp",
      "%s(pub=%p,payload=%p,len=%d)", __FUNCTION__, pub, payload, len)
    RCUTILS_LOG_DEBUG_NAMED(
      "rmw_dps_cpp",
      "pub={uuid=%s,sequenceNum=%d}", __FUNCTION__, DPS_UUIDToString(DPS_PublicationGetUUID(pub)), DPS_PublicationGetSequenceNum(pub));

    Listener * listener = (Listener *) DPS_GetPublicationData(pub);
    Data data = std::make_pair(Publication(DPS_CopyPublication(pub)), rmw_dps_cpp::cbor::RxStream(payload, len));
    std::lock_guard<std::mutex> lock(listener->internalMutex_);

    if (listener->conditionMutex_) {
      std::unique_lock<std::mutex> clock(*listener->conditionMutex_);
      // the change to data_ needs to be mutually exclusive with rmw_wait()
      // which checks hasData() and decides if wait() needs to be called
      listener->data_.push(std::move(data));
      clock.unlock();
      listener->conditionVariable_->notify_one();
    } else {
      listener->data_.push(std::move(data));
    }
  }

  void
  attachCondition(std::mutex * conditionMutex, std::condition_variable * conditionVariable)
  {
    std::lock_guard<std::mutex> lock(internalMutex_);
    conditionMutex_ = conditionMutex;
    conditionVariable_ = conditionVariable;
  }

  void
  detachCondition()
  {
    std::lock_guard<std::mutex> lock(internalMutex_);
    conditionMutex_ = nullptr;
    conditionVariable_ = nullptr;
  }

  bool
  hasData()
  {
    return data_.size() > 0;
  }

  bool
  takeNextData(rmw_dps_cpp::cbor::RxStream & buffer, Publication & pub)
  {
    std::lock_guard<std::mutex> lock(internalMutex_);
    if (data_.empty()) {
      return false;
    }
    Data & data = data_.front();
    pub = std::move(data.first);
    buffer = std::move(data.second);
    data_.pop();
    return true;
  }

private:
  std::mutex internalMutex_;
  std::queue<Data> data_;
  std::mutex * conditionMutex_;
  std::condition_variable * conditionVariable_;
};

#endif  // RMW_DPS_CPP__LISTENER_HPP_
