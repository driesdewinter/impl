include_directories(include)

add_executable(unit_test_binary perftest.cpp specials.cpp)
target_link_libraries(unit_test_binary gtest gtest_main)

add_test(unit_test_binary unit_test_binary)

add_subdirectory(failures)
add_subdirectory(readme-examples)
