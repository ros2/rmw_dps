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

#include "publish_common.hpp"

void
_published(
  DPS_Publication * pub,
  const DPS_Buffer * bufs,
  size_t numBufs,
  DPS_Status status,
  void * data)
{
  (void)pub;
  (void)bufs;
  (void)numBufs;
  DPS_SignalEvent(reinterpret_cast<DPS_Event *>(data), status);
}

DPS_Status
publish(DPS_Publication * pub, const uint8_t * data, size_t size)
{
  DPS_Buffer buf = {const_cast<uint8_t *>(data), size};
  DPS_Event * event = DPS_CreateEvent();
  if (!event) {
    return DPS_ERR_RESOURCES;
  }
  DPS_Status ret = DPS_PublishBufs(pub, &buf, 1, 0, _published, event);
  if (ret == DPS_OK) {
    ret = DPS_WaitForEvent(event);
  }
  DPS_DestroyEvent(event);
  return ret;
}
