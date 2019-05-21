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

project(rmw_dps_cpp)

# Default to C++14
if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 14)
endif()

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake_ros REQUIRED)

find_package(rcutils REQUIRED)

find_package(dps_for_iot_cmake_module REQUIRED)
find_package(dps_for_iot REQUIRED CONFIG)
find_package(dps_for_iot REQUIRED MODULE)

find_package(rmw REQUIRED)
find_package(rosidl_typesupport_introspection_c REQUIRED)
find_package(rosidl_typesupport_introspection_cpp REQUIRED)

include_directories(
  include
  ${dps_for_iot_INCLUDE_DIR})

add_library(rmw_dps_cpp
  src/client_service_common.cpp
  src/identifier.cpp
  src/names_common.cpp
  src/namespace_prefix.cpp
  src/rmw_client.cpp
  src/rmw_compare_gids_equal.cpp
  src/rmw_count.cpp
  src/rmw_get_gid_for_publisher.cpp
  src/rmw_get_implementation_identifier.cpp
  src/rmw_get_serialization_format.cpp
  src/rmw_guard_condition.cpp
  src/rmw_init.cpp
  src/rmw_logging.cpp
  src/rmw_node.cpp
  src/rmw_node_info_and_types.cpp
  src/rmw_node_names.cpp
  src/rmw_publish.cpp
  src/rmw_publisher.cpp
  src/rmw_request.cpp
  src/rmw_response.cpp
  src/rmw_serialize.cpp
  src/rmw_service.cpp
  src/rmw_service_names_and_types.cpp
  src/rmw_service_server_is_available.cpp
  src/rmw_subscription.cpp
  src/rmw_take.cpp
  src/rmw_trigger_guard_condition.cpp
  src/rmw_wait.cpp
  src/rmw_wait_set.cpp
  src/ros_message_serialization.cpp
  src/serialization_format.cpp
  src/type_support_common.cpp
)
target_link_libraries(rmw_dps_cpp
  dps_shared)

# Add the definitions, include directories and libraries of packages
# to a target
ament_target_dependencies(rmw_dps_cpp
  "rcutils"
  "rosidl_typesupport_introspection_c"
  "rosidl_typesupport_introspection_cpp"
  "rmw"
)

# Configures a library which implements the rmw interface with custom
# settings (the custom settings are all related to library symbol
# visibility)
configure_rmw_library(rmw_dps_cpp)

# Causes the visibility macros to use dllexport rather than dllimport,
# which is appropriate when building the dll but not consuming it.
target_compile_definitions(${PROJECT_NAME}
  PRIVATE "RMW_DPS_CPP_BUILDING_LIBRARY")

# Export include directories to downstream packages
ament_export_include_directories(include)
# Export libraries to downstream packages
ament_export_libraries(rmw_dps_cpp)

# Export dependencies to downstream packages
ament_export_dependencies(rosidl_typesupport_introspection_cpp)
ament_export_dependencies(rosidl_typesupport_introspection_c)
ament_export_dependencies(rcutils)
ament_export_dependencies(rmw)
ament_export_dependencies(dps_for_iot_cmake_module)
ament_export_dependencies(dps_for_iot)

# Register the current package as a ROS middleware implementation
# <language:typesupport> tuples where language is the language of the
# typesupport package and typesupport is the name of the package
register_rmw_implementation(
  "c:rosidl_typesupport_c:rosidl_typesupport_introspection_c"
  "cpp:rosidl_typesupport_cpp:rosidl_typesupport_introspection_cpp")

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  ament_lint_auto_find_test_dependencies()
endif()

ament_package(
  CONFIG_EXTRAS_POST "rmw_dps_cpp-extras.cmake"
)

install(
  DIRECTORY include/
  DESTINATION include
)

install(
  TARGETS rmw_dps_cpp
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)
