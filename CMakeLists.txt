# SPDX-License-Identifier: Apache-2.0
cmake_minimum_required(VERSION 3.31.6)

if(NOT CMAKE_VERSION VERSION_EQUAL "3.31.6")
  message(FATAL_ERROR "Use pinned CMake 3.31.6; see toolchains/build-tools.txt.")
endif()

project(Omniweft VERSION 0.1.0 LANGUAGES CXX)
include(cmake/ToolchainPolicy.cmake)

file(SHA256 "${CMAKE_CURRENT_SOURCE_DIR}/third_party/nlohmann/json.hpp" json_header_hash)
if(NOT json_header_hash STREQUAL "aaf127c04cb31c406e5b04a63f1ae89369fccde6d8fa7cdda1ed4f32dfc5de63")
  message(FATAL_ERROR "Vendored nlohmann/json 3.12.0 differs from its reviewed source hash.")
endif()
file(SHA256 "${CMAKE_CURRENT_SOURCE_DIR}/third_party/nlohmann/LICENSE.MIT" json_license_hash)
if(NOT json_license_hash STREQUAL "46a65cffd1ea955132d95a8dd921640714a8d6b537d2e4e482d31145ae95b603")
  message(FATAL_ERROR "The vendored JSON license must retain the reviewed upstream notice.")
endif()

function(ow_native_policy target)
  target_compile_features("${target}" PUBLIC cxx_std_20)
  set_target_properties("${target}" PROPERTIES CXX_EXTENSIONS OFF)
  if(MSVC)
    target_compile_options("${target}" PRIVATE /W4 /WX /permissive- /EHsc)
  else()
    target_compile_options("${target}" PRIVATE -Wall -Wextra -Wpedantic -Werror)
  endif()
endfunction()

add_library(ow_commands STATIC src/commands.cpp)
target_include_directories(ow_commands PUBLIC include)
target_include_directories(ow_commands SYSTEM PRIVATE third_party)
ow_native_policy(ow_commands)

add_library(ow_world STATIC src/world.cpp)
target_include_directories(ow_world PUBLIC include)
ow_native_policy(ow_world)

add_library(ow_transactions STATIC src/transactions.cpp)
target_link_libraries(ow_transactions PUBLIC ow_world ow_commands)
target_include_directories(ow_transactions PUBLIC include)
ow_native_policy(ow_transactions)

add_library(ow_observe STATIC src/observe.cpp)
target_link_libraries(ow_observe PUBLIC ow_world)
target_include_directories(ow_observe PUBLIC include)
target_include_directories(ow_observe SYSTEM PRIVATE third_party)
ow_native_policy(ow_observe)

add_library(ow_policy STATIC src/policy.cpp)
target_link_libraries(ow_policy PUBLIC ow_transactions)
target_include_directories(ow_policy PUBLIC include)
ow_native_policy(ow_policy)

add_library(ow_presentation STATIC src/presentation.cpp)
target_link_libraries(ow_presentation PUBLIC ow_world)
target_include_directories(ow_presentation PUBLIC include)
ow_native_policy(ow_presentation)

add_executable(omniweft_examples src/bootstrap_main.cpp src/protocol_example.cpp src/objects_example.cpp src/render_example.cpp src/observe_example.cpp)
target_link_libraries(omniweft_examples PRIVATE ow_transactions ow_presentation ow_observe)
target_include_directories(omniweft_examples SYSTEM PRIVATE third_party)
ow_native_policy(omniweft_examples)

find_package(Threads REQUIRED)
add_library(ow_control STATIC src/control.cpp)
target_link_libraries(ow_control PUBLIC ow_commands ow_policy PRIVATE Threads::Threads)
target_include_directories(ow_control PUBLIC include)
target_include_directories(ow_control SYSTEM PRIVATE third_party)
if(WIN32)
  target_link_libraries(ow_control PRIVATE ws2_32 bcrypt)
endif()
ow_native_policy(ow_control)

add_executable(omniweft_control src/control_main.cpp)
target_link_libraries(omniweft_control PRIVATE ow_control ow_transactions)
ow_native_policy(omniweft_control)

add_library(ow_simulation STATIC src/simulation.cpp)
target_include_directories(ow_simulation PUBLIC include)
ow_native_policy(ow_simulation)

add_executable(omniweft_policy src/policy_main.cpp)
target_link_libraries(omniweft_policy PRIVATE ow_control ow_policy ow_simulation ow_presentation Threads::Threads)
target_include_directories(omniweft_policy SYSTEM PRIVATE third_party)
ow_native_policy(omniweft_policy)

add_executable(omniweft_agents src/agents_main.cpp)
target_link_libraries(omniweft_agents PRIVATE ow_control ow_transactions ow_presentation ow_simulation Threads::Threads)
target_include_directories(omniweft_agents SYSTEM PRIVATE third_party)
ow_native_policy(omniweft_agents)

option(OW_ENABLE_VULKAN "Build the optional SDL/Vulkan presentation adapter" OFF)
if(OW_ENABLE_VULKAN)
  include(cmake/RenderDependencies.cmake)
  target_link_libraries(omniweft_policy PRIVATE ow_render)
  target_compile_definitions(omniweft_policy PRIVATE OW_ENABLE_VULKAN=1)
  target_link_libraries(omniweft_agents PRIVATE ow_render)
  target_compile_definitions(omniweft_agents PRIVATE OW_ENABLE_VULKAN=1)
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

  add_executable(policy_native_test tests/policy_native_test.cpp)
  target_link_libraries(policy_native_test PRIVATE ow_policy)
  ow_native_policy(policy_native_test)
  add_test(NAME policy.native COMMAND policy_native_test)
  add_test(NAME showcase.ai_blocks
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/showcase_oracle.py"
      --executable "$<TARGET_FILE:omniweft_policy>" --headless)
  add_test(NAME showcase.oracle_selftest
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/showcase_oracle_test.py")
  set_tests_properties(showcase.ai_blocks showcase.oracle_selftest PROPERTIES TIMEOUT 90)
  add_test(NAME policy.denied_edits
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/policy_oracle.py"
      --executable "$<TARGET_FILE:omniweft_policy>")
  set_tests_properties(policy.denied_edits PROPERTIES TIMEOUT 90)
  set_tests_properties(policy.native PROPERTIES TIMEOUT 30)

  add_executable(protocol_native_test tests/protocol_native_test.cpp)
  target_link_libraries(protocol_native_test PRIVATE ow_commands)
  ow_native_policy(protocol_native_test)
  add_test(NAME protocol.native COMMAND protocol_native_test)
  add_test(NAME protocol.reject_invalid
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/protocol_oracle.py"
      --executable "$<TARGET_FILE:omniweft_examples>")
  set_tests_properties(protocol.native protocol.reject_invalid PROPERTIES TIMEOUT 60)

  add_executable(observe_legacy_fixture tests/hierarchy_host_fixture.cpp)
  target_link_libraries(observe_legacy_fixture PRIVATE ow_control)
  target_compile_definitions(observe_legacy_fixture PRIVATE OW_TEST_TAGS=1)
  ow_native_policy(observe_legacy_fixture)
  add_executable(observe_policy_fixture tests/hierarchy_host_fixture.cpp)
  target_link_libraries(observe_policy_fixture PRIVATE ow_control)
  target_compile_definitions(observe_policy_fixture PRIVATE OW_TEST_TAGS=1 OW_TEST_POLICY=1)
  ow_native_policy(observe_policy_fixture)
  add_executable(observe_native_test tests/observe_native_test.cpp)
  target_link_libraries(observe_native_test PRIVATE ow_observe ow_policy)
  target_include_directories(observe_native_test SYSTEM PRIVATE third_party)
  ow_native_policy(observe_native_test)
  add_test(NAME observe.native COMMAND observe_native_test)
  add_test(NAME observe.semantic_query
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/observe_oracle.py"
      --executable "$<TARGET_FILE:omniweft_examples>" --control "$<TARGET_FILE:omniweft_control>"
      --agents "$<TARGET_FILE:omniweft_agents>" --policy "$<TARGET_FILE:omniweft_policy>"
      --tag-legacy "$<TARGET_FILE:observe_legacy_fixture>" --tag-policy "$<TARGET_FILE:observe_policy_fixture>")
  add_test(NAME observe.oracle_selftest
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/observe_oracle_test.py")
  set_tests_properties(observe.native observe.semantic_query observe.oracle_selftest PROPERTIES TIMEOUT 120)

  add_executable(hierarchy_legacy_fixture tests/hierarchy_host_fixture.cpp)
  target_link_libraries(hierarchy_legacy_fixture PRIVATE ow_control)
  ow_native_policy(hierarchy_legacy_fixture)
  add_executable(hierarchy_policy_fixture tests/hierarchy_host_fixture.cpp)
  target_link_libraries(hierarchy_policy_fixture PRIVATE ow_control)
  target_compile_definitions(hierarchy_policy_fixture PRIVATE OW_TEST_POLICY=1)
  ow_native_policy(hierarchy_policy_fixture)
  add_executable(hierarchy_native_test tests/hierarchy_native_test.cpp)
  target_link_libraries(hierarchy_native_test PRIVATE ow_transactions ow_presentation ow_policy)
  ow_native_policy(hierarchy_native_test)
  add_test(NAME objects.hierarchy_native COMMAND hierarchy_native_test)
  add_test(NAME objects.hierarchy
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/hierarchy_oracle.py"
      --executable "$<TARGET_FILE:omniweft_examples>" --control "$<TARGET_FILE:omniweft_control>"
      --agents "$<TARGET_FILE:omniweft_agents>" --policy "$<TARGET_FILE:omniweft_policy>"
      --hierarchy-legacy "$<TARGET_FILE:hierarchy_legacy_fixture>" --hierarchy-policy "$<TARGET_FILE:hierarchy_policy_fixture>")
  add_test(NAME objects.hierarchy_oracle_selftest
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/hierarchy_oracle_test.py")
  set_tests_properties(objects.hierarchy_native objects.hierarchy objects.hierarchy_oracle_selftest PROPERTIES TIMEOUT 120)

  add_executable(objects_native_test tests/objects_native_test.cpp)
  target_link_libraries(objects_native_test PRIVATE ow_transactions)
  ow_native_policy(objects_native_test)
  add_test(NAME objects.native COMMAND objects_native_test)
  add_test(NAME objects.atomic
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/objects_oracle.py"
      --executable "$<TARGET_FILE:omniweft_examples>")
  set_tests_properties(objects.native objects.atomic PROPERTIES TIMEOUT 120)

  add_executable(presentation_native_test tests/presentation_native_test.cpp)
  target_link_libraries(presentation_native_test PRIVATE ow_presentation ow_transactions)
  ow_native_policy(presentation_native_test)
  add_test(NAME presentation.native COMMAND presentation_native_test)
  add_test(NAME render.structural
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/render_oracle.py"
      --executable "$<TARGET_FILE:omniweft_examples>" --headless)
  add_test(NAME render.oracle_selftest
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/render_oracle_test.py")
  set_tests_properties(presentation.native render.structural render.oracle_selftest PROPERTIES TIMEOUT 120)

  add_test(NAME sdk.move_cube
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/sdk_oracle.py"
      --executable "$<TARGET_FILE:omniweft_control>")
  add_test(NAME sdk.oracle_selftest
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/sdk_oracle_test.py")
  set_tests_properties(sdk.move_cube sdk.oracle_selftest PROPERTIES TIMEOUT 180)
  add_executable(simulation_native_test tests/simulation_native_test.cpp)
  target_link_libraries(simulation_native_test PRIVATE ow_simulation)
  ow_native_policy(simulation_native_test)
  add_test(NAME simulation.native COMMAND simulation_native_test)
  add_test(NAME agents.mock_builder
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/agents_oracle.py"
      --executable "$<TARGET_FILE:omniweft_agents>" --headless)
  add_test(NAME agents.oracle_selftest
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/agents_oracle_test.py")
  set_tests_properties(simulation.native agents.mock_builder agents.oracle_selftest PROPERTIES TIMEOUT 180)
  add_test(NAME agents.sdk_boundary
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/agents_sdk_test.py"
      --executable "$<TARGET_FILE:omniweft_agents>")
  set_tests_properties(agents.sdk_boundary PROPERTIES TIMEOUT 90)
  add_test(NAME agents.sdk_compatibility
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/sdk_oracle.py"
      --executable "$<TARGET_FILE:omniweft_agents>")
  set_tests_properties(agents.sdk_compatibility PROPERTIES TIMEOUT 180)
endif()
