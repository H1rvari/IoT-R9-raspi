cmake_minimum_required(VERSION 3.16)
project(IoT-R9-raspi)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(external/SimpleBLE/simpleble)
find_package(simpleble REQUIRED CONFIG)
add_executable(IoT-R9-raspi src/main.cpp)
target_link_libraries(IoT-R9-raspi PRIVATE simpleble::simpleble)