CMAKE_MINIMUM_REQUIRED(VERSION 3.10 FATAL_ERROR)

PROJECT(driver)

set(CMAKE_EXPORT_COMPILE_COMMANDS on)

SET(Boost_USE_STATIC_LIBS ON)
find_package(Boost 1.74.0 REQUIRED system log log_setup thread)

add_executable(driver driver.cpp cbor11.cpp)
target_link_libraries(driver PUBLIC pthread Boost::log_setup Boost::log) 