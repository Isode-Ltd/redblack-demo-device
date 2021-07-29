CMAKE_MINIMUM_REQUIRED(VERSION 3.10 FATAL_ERROR)

PROJECT(isode-demo-radio-driver)

set(CMAKE_EXPORT_COMPILE_COMMANDS on)

SET(Boost_USE_STATIC_LIBS ON)
find_package(Boost 1.74.0 REQUIRED system log log_setup thread program_options)

add_executable(isode-demo-radio-driver driver.cpp cbor11.cpp)
target_link_libraries(isode-demo-radio-driver PUBLIC Boost::log_setup Boost::log Boost::program_options)
