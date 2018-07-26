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

#include "rcutils/logging_macros.h"

#include <dps/dbg.h>

#include "rmw/rmw.h"

extern "C"
{
rmw_ret_t
rmw_init()
{
  rcutils_ret_t ret =rcutils_logging_set_logger_level("rmw_dps_cpp", RCUTILS_LOG_SEVERITY_DEBUG);
  (void)ret;
  DPS_Debug = 0;

  RCUTILS_LOG_DEBUG_NAMED(
    "rmw_dps_cpp",
    "%s()", __FUNCTION__)

  return RMW_RET_OK;
}
}  // extern "C"
