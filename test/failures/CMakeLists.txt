# include(CTest)

function(TestCompilerError TEST_NAME EXPECTED_ERROR)
    add_executable(${TEST_NAME} EXCLUDE_FROM_ALL ${TEST_NAME}.cpp)
    add_test(NAME ${TEST_NAME}
             COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target ${TEST_NAME})
	set_tests_properties(${TEST_NAME} PROPERTIES PASS_REGULAR_EXPRESSION ${EXPECTED_ERROR})
endfunction()

TestCompilerError(fail1 "T is not a base of U")
TestCompilerError(fail2 "U is not move-constructible")
TestCompilerError(fail3 "capacity too small to store U")
TestCompilerError(fail4 "U is not move-constructible")
TestCompilerError(fail5 "alignment too small to store U")
TestCompilerError(fail6 "T is not a base of U")
TestCompilerError(fail7 "T is not a base of U")
TestCompilerError(fail8 "T is not a base of U")
TestCompilerError(fail9 "cannot convert between different alignments")
TestCompilerError(fail10 "cannot convert between different capacities")
