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

cmake_minimum_required(VERSION 3.5)

project(dps_for_iot_cmake_module)
set(PROJECT_VERSION 0.9.0)
set(INCLUDE_INSTALL_DIR ${CMAKE_INSTALL_PREFIX}/include/ CACHE PATH "Installation directory for C++ headers")
set(LIB_INSTALL_DIR ${CMAKE_INSTALL_PREFIX}/lib/ CACHE PATH "Installation directory for libraries")
set(BIN_INSTALL_DIR ${CMAKE_INSTALL_PREFIX}/bin/ CACHE PATH "Installation directory for binaries")

find_package(ament_cmake REQUIRED)

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  # Do not lint CMake files.
  set(ament_cmake_lint_cmake_FOUND TRUE)
  ament_lint_auto_find_test_dependencies()
endif()

ament_package(
  CONFIG_EXTRAS "dps_for_iot_cmake_module-extras.cmake"
)

include(ExternalProject)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(VARIANT "debug")
else()
  set(VARIANT "release")
endif()
if(MSVC)
  set(DEF_FILE "rmw_dps_shared.def")
else()
  set(DEF_FILE "librmw_dps_shared.map")
endif()
ExternalProject_Add(dps_for_iot
  PREFIX dps_for_iot
  URL https://github.com/intel/dps-for-iot/archive/master.zip
  CONFIGURE_COMMAND ""
  BUILD_COMMAND scons -C ${CMAKE_CURRENT_BINARY_DIR}/dps_for_iot/src/dps_for_iot variant=${VARIANT} transport=udp bindings=none DEF_FILE=${CMAKE_CURRENT_SOURCE_DIR}/${DEF_FILE}
  BUILD_ALWAYS TRUE
  INSTALL_COMMAND "")

ExternalProject_Get_Property(dps_for_iot SOURCE_DIR)

install(DIRECTORY ${SOURCE_DIR}/build/dist/inc/
  DESTINATION ${INCLUDE_INSTALL_DIR}
  COMPONENT headers)

if(MSVC)
  install(FILES
          ${SOURCE_DIR}/build/dist/lib/dps_shared.lib
          ${SOURCE_DIR}/build/dist/lib/dps_shared.exp
    DESTINATION ${LIB_INSTALL_DIR}
    COMPONENT libraries)
  install(FILES
          ${SOURCE_DIR}/build/dist/lib/dps_shared.dll
    DESTINATION ${BIN_INSTALL_DIR}
    COMPONENT libraries)
else()
  install(DIRECTORY ${SOURCE_DIR}/build/dist/lib/
    DESTINATION ${LIB_INSTALL_DIR}
    USE_SOURCE_PERMISSIONS
    COMPONENT libraries)
endif()

include(CMakePackageConfigHelpers)
configure_package_config_file(${CMAKE_CURRENT_SOURCE_DIR}/dps_for_iot-config.cmake.in
  ${PROJECT_BINARY_DIR}/dps_for_iot-config.cmake
  INSTALL_DESTINATION share/${PROJECT_NAME}/cmake
  PATH_VARS INCLUDE_INSTALL_DIR LIB_INSTALL_DIR)
write_basic_package_version_file(${PROJECT_BINARY_DIR}/dps_for_iot-config-version.cmake
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion)
install(FILES ${PROJECT_BINARY_DIR}/dps_for_iot-config.cmake
  ${PROJECT_BINARY_DIR}/dps_for_iot-config-version.cmake
  DESTINATION share/${PROJECT_NAME}/cmake
  COMPONENT cmake)

install(DIRECTORY cmake
  DESTINATION share/${PROJECT_NAME})
