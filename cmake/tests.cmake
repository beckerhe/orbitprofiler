# Copyright (c) 2020 The Orbit Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# The register_test function automatically registers a test with CTest.
# So it will be automatically executed in the CI and a xUnit-based XML
# report will be generated.
#
# Usage examples:
# - default:
#  register_test(OrbitQtTests)
#
# - with test isolation:
#  register_test(OrbitQtTests SUBTESTS Test1 Test2)
#    Unfortunately test isolation requires to list all
#    GTest-tests in the SUBTESTS option.
#
# - with custom timeout:
#  register_test(OrbitQtTests TIMEOUT 120)
#
# - combined:
#  register_test(OrbitQtTests SUBTESTS Test1 Test2 TIMEOUT 120)

function(register_test TEST_TARGET)
  cmake_parse_arguments(ARGS "" "TIMEOUT" "SUBTESTS" ${ARGN})
  string(REGEX REPLACE "Tests$" "" TEST_NAME "${TEST_TARGET}")

  set(TESTRESULTS_DIRECTORY "${CMAKE_BINARY_DIR}/testresults")

  if(NOT ARGS_SUBTESTS)
    set(ARGS_SUBTESTS "*")
  endif()

  set(TESTS "")

  foreach(SUBTEST ${ARGS_SUBTESTS})
    if ("${SUBTEST}" STREQUAL "*")
      set(CURRENT_TEST_NAME "${TEST_NAME}")
    else()
      set(CURRENT_TEST_NAME "${TEST_NAME}.${SUBTEST}")
    endif()

    add_test(
      NAME "${CURRENT_TEST_NAME}"
      COMMAND
        "$<TARGET_FILE:${TEST_TARGET}>"
        "--gtest_filter=${SUBTEST}"
        "--gtest_output=xml:${TESTRESULTS_DIRECTORY}/${CURRENT_TEST_NAME}_sponge_log.xml")

    if(ARGS_TIMEOUT)
      set_tests_properties(${CURRENT_TEST_NAME} PROPERTIES TIMEOUT ${ARGS_TIMEOUT})
    else()
      set_tests_properties(${CURRENT_TEST_NAME} PROPERTIES TIMEOUT 30)
    endif()

    list(APPEND TESTS "${CURRENT_TEST_NAME}")
  endforeach()

  set(${TEST_TARGET} ${TESTS} PARENT_SCOPE)
endfunction()

if(NOT TARGET GTest::GTest)
  add_library(GTest::GTest INTERFACE IMPORTED)
  target_link_libraries(GTest::GTest INTERFACE CONAN_PKG::gtest)
endif()

if(NOT TARGET GTest::Main)
  set(FILE_PATH ${CMAKE_BINARY_DIR}/test_main.cpp)
  if(NOT EXISTS ${FILE_PATH})
    set(GTEST_MAIN_CPP "\
#include <cstdio>\n\
#include <gtest/gtest.h>\n\
\n\
int main(int argc, char* argv[]) {\n\
  printf(\"Running main() from %s\\n\", __FILE__)\;\n\
  ::testing::InitGoogleTest(&argc, argv)\;\n\
  return RUN_ALL_TESTS()\;\n\
}\n\
")

    file(WRITE ${FILE_PATH} ${GTEST_MAIN_CPP})
  endif()

  add_library(GTest_Main STATIC EXCLUDE_FROM_ALL ${FILE_PATH})
  target_link_libraries(GTest_Main PUBLIC GTest::GTest)
  add_library(GTest::Main ALIAS GTest_Main)
endif()
