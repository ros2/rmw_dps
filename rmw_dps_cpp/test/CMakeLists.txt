find_package(ament_cmake_gmock REQUIRED)

ament_add_gmock(test_node
  test_node.cpp
  # Append the directory of librmw_dps_cpp so it is found at test time.
  APPEND_LIBRARY_DIRS "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
)
if(TARGET test_node)
  target_link_libraries(test_node ${PROJECT_NAME})
endif()
