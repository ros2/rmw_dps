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

###############################################################################
#
# CMake module for finding Intel DPS.
#
# Output variables:
#
# - dps_for_iot_FOUND: flag indicating if the package was found
# - dps_for_iot_INCLUDE_DIR: Paths to the header files
#
# Example usage:
#
#   find_package(dps_for_iot_cmake_module REQUIRED)
#   find_package(dps_for_iot MODULE)
#   # use dps_for_iot_* variables
#
###############################################################################

# lint_cmake: -convention/filename, -package/stdargs

set(dps_for_iot_FOUND FALSE)

find_path(dps_for_iot_INCLUDE_DIR
  NAMES dps/)

find_package(dps_for_iot REQUIRED CONFIG)

string(REGEX MATCH "^[0-9]+\\.[0-9]+" dps_for_iot_MAJOR_MINOR_VERSION "${dps_for_iot_VERSION}")

find_library(dps_for_iot_LIBRARY
  NAMES dps_shared-${dps_for_iot_MAJOR_MINOR_VERSION} dps_shared)

set(dps_for_iot_LIBRARIES
  ${dps_for_iot_LIBRARY}
  )

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(dps_for_iot
  FOUND_VAR dps_for_iot_FOUND
  REQUIRED_VARS
    dps_for_iot_INCLUDE_DIR
    dps_for_iot_LIBRARIES
)
