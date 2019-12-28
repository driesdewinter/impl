cmake_minimum_required (VERSION 2.6)

project (impl)

include_directories(include)

set(CMAKE_CXX_FLAGS "-std=c++17 -O3 -Wall -Wextra -g -pthread")

enable_testing()

add_subdirectory(test)

include(GNUInstallDirs)

install(FILES include/ddw/impl.hpp
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/ddw/)
