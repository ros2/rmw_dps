#!/bin/bash
set -e

docker pull osrf/ros2:nightly
docker run -dit -v "${TRAVIS_BUILD_DIR}/..:/shared"  --network host --name=osrf_ros2_nightly --privileged osrf/ros2:nightly /bin/bash
docker exec osrf_ros2_nightly /bin/bash -c "git config --global user.name nobody"
docker exec osrf_ros2_nightly /bin/bash -c "git config --global user.email noreply@osrfoundation.org"

docker exec osrf_ros2_nightly /bin/bash -c 'apt-get update && source /opt/ros/`rosversion -d`/setup.bash && rosdep update && rosdep install --from-paths /shared/rmw_dps --ignore-src -r -y'
docker exec osrf_ros2_nightly /bin/bash -c 'source /opt/ros/`rosversion -d`/setup.bash && cd /shared/rmw_dps && colcon build --packages-up-to rmw_dps_cpp'
docker exec osrf_ros2_nightly /bin/bash -c 'source /opt/ros/`rosversion -d`/setup.bash && cd /shared/rmw_dps && colcon test'
docker exec osrf_ros2_nightly /bin/bash -c 'source /opt/ros/`rosversion -d`/setup.bash && cd /shared/rmw_dps && colcon test-result --verbose'
