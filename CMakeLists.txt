cmake_minimum_required(VERSION 3.15)
project(ZigbeeStack LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

# Add include directory
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

add_library(zoh
        src/platform/RawSpinelDriver.cpp
        src/platform/SerialPort.cpp
        src/SecurityManager.cpp
        src/ZigbeeNwk.cpp
        src/ZigbeeZcl.cpp
        src/ZigbeeZdo.cpp
        src/ZigbeeAps.cpp
        src/ZigbeeMac.cpp
        src/ZdoEndpointTable.cpp
        src/ZigbeeMacHeader.cpp
        src/ZigbeeNwkHeader.cpp
)
target_include_directories(zoh PUBLIC include)

# Example coordinator main
add_executable(coordinator
        examples/coordinator.cpp
)
target_link_libraries(coordinator PRIVATE zoh)

# ...
