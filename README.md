# rmw_dps [![Build Status](https://travis-ci.com/ros2/rmw_dps.svg?branch=master)](https://travis-ci.com/ros2/rmw_dps)

Implementation of the ROS Middleware (rmw) Interface using Intel's [Distributed Publish &amp; Subscribe for IoT](https://github.com/intel/dps-for-iot) (DPS).

DPS is a new protocol that implements the publish/subscribe (pub/sub) communication pattern.  For more information see the project's [documentation](https://intel.github.io/dps-for-iot/).

## Building
This project adds two build targets: `dps_for_iot_cmake_module` and `rmw_dps_cpp`.  `dps_for_iot_cmake_module` builds the DPS libraries, and requires the `SCons` and `python-config` tools (make sure to run `rosdep install` as usual).

`rmw_dps_cpp` is the rmw implementation.  To build, either add rmw_dps to to your local ros2.repos or explicitly clone it.

For the first option, add the following block to ros2.repos and follow the ROS 2 [instructions](https://github.com/ros2/ros2/wiki/Maintaining-a-Source-Checkout) for updating and building.
```
  ros2/rmw_dps:
    type: git
    url: https://github.com/ros2/rmw_dps.git
    version: master
```

For the second option, clone this repository using the command below and build ROS 2.
```
git clone https://github.com/ros2/rmw_dps src/ros2/rmw_dps
```

## Implementation status
Work is ongoing to complete full support for all rmw APIs.

Completed:
- Publishers and subscribers, services and clients
- Message serialization and deserialization

To be completed:
- Full support of QoS
- Discoverability
- Pass through security configuration to DPS
