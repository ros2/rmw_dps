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

#include <algorithm>
#include <string>
#include <vector>

#include "rcpputils/find_and_replace.hpp"
#include "rcutils/logging_macros.h"
#include "rcutils/types.h"

#include "rmw_dps_cpp/namespace_prefix.hpp"

/// Return the demangled ROS type or the original if not a ROS type.
std::string
_demangle_if_ros_type(const std::string & dps_type_string)
{
  if (dps_type_string[dps_type_string.size() - 1] != '_') {
    // not a ROS type
    return dps_type_string;
  }

  std::string substring = "dps_::";
  size_t substring_position = dps_type_string.find(substring);
  if (substring_position == std::string::npos) {
    // not a ROS type
    return dps_type_string;
  }

  std::string type_namespace = dps_type_string.substr(0, substring_position);
  type_namespace = rcpputils::find_and_replace(type_namespace, "::", "/");
  size_t start = substring_position + substring.size();
  std::string type_name = dps_type_string.substr(start, dps_type_string.length() - 1 - start);
  return type_namespace + type_name;
}

/// Return the demangled service type if it is a ROS srv type, else "".
std::string
_demangle_service_type_only(const std::string & dps_type_name)
{
  std::string ns_substring = "dps_::";
  size_t ns_substring_position = dps_type_name.find(ns_substring);
  if (std::string::npos == ns_substring_position) {
    // not a ROS service type
    return "";
  }
  auto suffixes = {
    std::string("_Response_"),
    std::string("_Request_"),
  };
  std::string found_suffix = "";
  size_t suffix_position = 0;
  for (auto suffix : suffixes) {
    suffix_position = dps_type_name.rfind(suffix);
    if (suffix_position != std::string::npos) {
      if (dps_type_name.length() - suffix_position - suffix.length() != 0) {
        RCUTILS_LOG_WARN_NAMED("rmw_dps_cpp",
          "service type contains 'dps_::' and a suffix, but not at the end"
          ", report this: '%s'", dps_type_name.c_str());
        continue;
      }
      found_suffix = suffix;
      break;
    }
  }
  if (std::string::npos == suffix_position) {
    RCUTILS_LOG_WARN_NAMED("rmw_dps_cpp",
      "service type contains 'dps_::' but does not have a suffix"
      ", report this: '%s'", dps_type_name.c_str());
    return "";
  }
  // everything checks out, reformat it from '[type_namespace::]dps_::<type><suffix>'
  // to '[type_namespace/]<type>'
  std::string type_namespace = dps_type_name.substr(0, ns_substring_position);
  type_namespace = rcpputils::find_and_replace(type_namespace, "::", "/");
  size_t start = ns_substring_position + ns_substring.length();
  std::string type_name = dps_type_name.substr(start, suffix_position - start);
  return type_namespace + type_name;
}
