cmake_minimum_required (VERSION 2.6)

project (impl)

include_directories(include)

set (CMAKE_CXX_FLAGS "-std=c++17 -O3 -Wall -Wextra -pthread -g")

add_executable(trials trials.cpp)
target_link_libraries(trials dl)

add_executable(unit_test_binary test/perftest.cpp)
target_link_libraries(unit_test_binary gtest gtest_main)

add_test(unit_tests unit_test_binary)
