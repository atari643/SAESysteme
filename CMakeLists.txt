cmake_minimum_required(VERSION 3.5)

PROJECT("TESTFW" C)

set(CMAKE_VERBOSE_MAKEFILE ON)

include(CTest)
enable_testing()

set(CMAKE_C_FLAGS "-Wall -std=c99")
set(CMAKE_LD_FLAGS "-rdynamic")

add_library(testfw testfw.c testfw.h)
target_link_libraries(testfw dl)

add_library(testfw_main testfw_main.c testfw.h)
target_link_libraries(testfw_main testfw)

add_executable(hello hello.c)
target_link_libraries(hello testfw_main testfw)

add_executable(sample sample.c sample.h)
target_link_libraries(sample testfw_main testfw)

add_executable(sample_main sample_main.c sample.c sample.h)
target_link_libraries(sample_main testfw)

# launch test directly using CTest
set(tests "test.success" "test.failure" "test.segfault" "test.assert" "test.sleep" "test.alarm" "test.args" "test.infiniteloop")
set(results "SUCCESS" "FAILURE" "KILLED" "KILLED" "TIMEOUT" "KILLED" "SUCCESS" "TIMEOUT")
list(LENGTH tests nbtests)
math(EXPR start "0")
math(EXPR end "${nbtests} - 1")

foreach(index RANGE ${start} ${end})
list(GET tests ${index} test)
list(GET results ${index} result)
# message(STATUS "Add test ${index} named \"${test}\" with expected result \"${result}\"")
add_test(${test} sample -t 2 -r ${test} -x)
set_tests_properties(${test} PROPERTIES PASS_REGULAR_EXPRESSION ${result} TIMEOUT 4)
endforeach()

# list tests within TESTFW
add_test(sample_list_test bash -c "${CMAKE_CURRENT_BINARY_DIR}/sample -R test -l | wc -l")
set_tests_properties(sample_list_test PROPERTIES PASS_REGULAR_EXPRESSION "9" TIMEOUT 1)
add_test(sample_list_othertest bash -c "${CMAKE_CURRENT_BINARY_DIR}/sample -R othertest -l | wc -l")
set_tests_properties(sample_list_othertest PROPERTIES PASS_REGULAR_EXPRESSION "2" TIMEOUT 1)
add_test(sample_list_onetest sample -r test.hello -l)
set_tests_properties(sample_list_onetest PROPERTIES PASS_REGULAR_EXPRESSION "test.hello" TIMEOUT 1)

# run all tests within TESTFW
add_test(sample_run_all bash -c "${CMAKE_CURRENT_BINARY_DIR}/sample -R test -t 2 -x")
set_tests_properties(sample_run_all PROPERTIES TIMEOUT 30)

# other test with TESTFW
add_test(sample_main sample_main)
set_tests_properties(sample_main PROPERTIES TIMEOUT 5)

# EOF