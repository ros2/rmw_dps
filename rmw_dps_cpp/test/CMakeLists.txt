find_package(ament_cmake_gmock REQUIRED)
find_package(test_msgs REQUIRED)

foreach(TEST test_node test_publisher test_subscription)
  ament_add_gmock(${TEST}
    ${TEST}.cpp
    # Append the directory of librmw_dps_cpp so it is found at test time.
    APPEND_LIBRARY_DIRS "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
  )
  ament_target_dependencies(${TEST}
    test_msgs
  )
  if(TARGET ${TEST})
    target_link_libraries(${TEST} ${PROJECT_NAME})
  endif()
endforeach()
