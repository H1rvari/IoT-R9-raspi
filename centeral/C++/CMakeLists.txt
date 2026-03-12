cmake_minimum_required(VERSION 3.16)
project(IoT-R9-raspi)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GPIOD REQUIRED libgpiodcxx libgpiod)

add_executable(IoT-R9-raspi src/main.cpp)

target_include_directories(IoT-R9-raspi PRIVATE ${GPIOD_INCLUDE_DIRS})
target_link_libraries(IoT-R9-raspi PRIVATE ${GPIOD_LIBRARIES})

add_subdirectory(external/SimpleBLE/simpleble)
target_link_libraries(IoT-R9-raspi PRIVATE simpleble::simpleble)