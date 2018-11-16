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

#ifndef RMW_DPS_CPP__SUBSCRIBER_LISTENER_HPP_
#define RMW_DPS_CPP__SUBSCRIBER_LISTENER_HPP_

#include <atomic>
#include <condition_variable>
#include <mutex>

#include <dps/dps.h>
#include <dps/Subscriber.hpp>

class SubscriberListener : public dps::SubscriberListener
{
public:
  explicit SubscriberListener()
  : data_(0),
    conditionMutex_(nullptr), conditionVariable_(nullptr)
  {
  }

  void
  onNewPublication(dps::Subscriber * sub)
  {
    std::lock_guard<std::mutex> lock(internalMutex_);

    if (conditionMutex_ != nullptr) {
      std::unique_lock<std::mutex> clock(*conditionMutex_);
      // the change to data_ needs to be mutually exclusive with rmw_wait()
      // which checks hasData() and decides if wait() needs to be called
      data_ = sub->unreadCount();
      clock.unlock();
      conditionVariable_->notify_one();
    } else {
      data_ = sub->unreadCount();
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
    return data_ > 0;
  }

  void
  data_taken()
  {
    std::lock_guard<std::mutex> lock(internalMutex_);

    if (conditionMutex_ != nullptr) {
      std::unique_lock<std::mutex> clock(*conditionMutex_);
      --data_;
    } else {
      --data_;
    }
  }

private:
  std::mutex internalMutex_;
  std::atomic_size_t data_;
  std::mutex * conditionMutex_;
  std::condition_variable * conditionVariable_;
};

#endif  // RMW_DPS_CPP__SUBSCRIBER_LISTENER_HPP_
