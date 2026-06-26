cmake_minimum_required(VERSION 3.16)

project(rivetmesh VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(RIVETMESH_BUILD_FUZZERS "Build local libFuzzer targets with Clang" OFF)

add_library(rivetmesh STATIC
  src/analyzer.cc
  src/bundle.cc
  src/checksum.cc
  src/profile.cc
  src/reader.cc
  src/route.cc
  src/rules.cc
  src/status.cc
  src/stencil.cc
  src/table.cc
)

target_include_directories(rivetmesh PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(rivetmesh PUBLIC cxx_std_17)

if(MSVC)
  target_compile_options(rivetmesh PRIVATE /W4 /permissive-)
else()
  target_compile_options(rivetmesh PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(rm_inspect tools/rm_inspect.cc)
target_link_libraries(rm_inspect PRIVATE rivetmesh)

add_executable(rivetmesh_tests tests/unit_tests.cc)
target_link_libraries(rivetmesh_tests PRIVATE rivetmesh)

enable_testing()
add_test(NAME rivetmesh_tests COMMAND rivetmesh_tests)

if(RIVETMESH_BUILD_FUZZERS)
  if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(FATAL_ERROR "RIVETMESH_BUILD_FUZZERS expects Clang or clang-cl")
  endif()

  target_compile_options(rivetmesh PRIVATE -fsanitize=fuzzer-no-link,address,undefined)

  add_executable(rivetmesh_bundle_fuzzer fuzz/rivetmesh_bundle_fuzzer.cc)
  target_link_libraries(rivetmesh_bundle_fuzzer PRIVATE rivetmesh)
  target_compile_options(rivetmesh_bundle_fuzzer PRIVATE -fsanitize=fuzzer,address,undefined)
  target_link_options(rivetmesh_bundle_fuzzer PRIVATE -fsanitize=fuzzer,address,undefined)

  add_executable(rivetmesh_rules_fuzzer fuzz/rivetmesh_rules_fuzzer.cc)
  target_link_libraries(rivetmesh_rules_fuzzer PRIVATE rivetmesh)
  target_compile_options(rivetmesh_rules_fuzzer PRIVATE -fsanitize=fuzzer,address,undefined)
  target_link_options(rivetmesh_rules_fuzzer PRIVATE -fsanitize=fuzzer,address,undefined)
endif()
