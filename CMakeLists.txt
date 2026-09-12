# SPDX-License-Identifier: Apache-2.0
cmake_minimum_required(VERSION 3.31.6)

if(NOT CMAKE_VERSION VERSION_EQUAL "3.31.6")
  message(FATAL_ERROR "Use pinned CMake 3.31.6; see toolchains/build-tools.txt.")
endif()

project(Omniweft VERSION 0.1.0 LANGUAGES CXX)
include(cmake/ToolchainPolicy.cmake)

add_executable(omniweft_examples src/bootstrap_main.cpp)
target_compile_features(omniweft_examples PRIVATE cxx_std_20)
set_target_properties(omniweft_examples PROPERTIES CXX_EXTENSIONS OFF)
if(MSVC)
  target_compile_options(omniweft_examples PRIVATE /W4 /WX /permissive- /EHsc)
else()
  target_compile_options(omniweft_examples PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

include(CTest)
if(BUILD_TESTING)
  find_package(Python3 3.12 REQUIRED COMPONENTS Interpreter)
  if(NOT Python3_VERSION VERSION_EQUAL "3.12.10" AND NOT Python3_VERSION VERSION_EQUAL "3.12.14")
    message(FATAL_ERROR "Use Python 3.12.10 (CI) or pinned local alternate 3.12.14; found ${Python3_VERSION}.")
  endif()
  add_test(NAME platform.bootstrap
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/bootstrap_oracle.py"
      --executable "$<TARGET_FILE:omniweft_examples>")
  add_test(NAME platform.preflight
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/preflight_test.py")
  set_tests_properties(platform.bootstrap platform.preflight PROPERTIES TIMEOUT 30)
endif()
