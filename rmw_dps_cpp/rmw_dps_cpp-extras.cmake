# Copyright 2018 Intel Corporation All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# copied from rmw_dps_cpp/rmw_dps_cpp-extras.cmake

find_package(dps_for_iot_cmake_module REQUIRED)
find_package(dps_for_iot REQUIRED CONFIG)
find_package(dps_for_iot REQUIRED MODULE)

list(APPEND rmw_dps_cpp_INCLUDE_DIRS ${dps_for_iot_INCLUDE_DIR})
# specific order: dependents before dependencies
list(APPEND rmw_dps_cpp_LIBRARIES dps_shared)
