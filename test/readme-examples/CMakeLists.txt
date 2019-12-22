add_executable(orig orig.cpp)
add_executable(sol1 sol1.cpp)
add_executable(sol2 sol2.cpp)

set(command-suffix " | diff -y ${CMAKE_CURRENT_SOURCE_DIR}/expected.txt -") 

add_test(readme-example-orig sh -c "${CMAKE_CURRENT_BINARY_DIR}/orig ${command-suffix}")
add_test(readme-example-sol1 sh -c "${CMAKE_CURRENT_BINARY_DIR}/sol1 ${command-suffix}")
add_test(readme-example-sol2 sh -c "${CMAKE_CURRENT_BINARY_DIR}/sol2 ${command-suffix}")
