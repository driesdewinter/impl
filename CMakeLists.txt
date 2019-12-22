cmake_minimum_required (VERSION 2.6)

project (impl)

include_directories(include)

set(CMAKE_CXX_FLAGS "-std=c++17 -O3 -Wall -Wextra -g -pthread")

add_executable(trials trials.cpp)
target_link_libraries(trials dl)

enable_testing()

add_subdirectory(test)
