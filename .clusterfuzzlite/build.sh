#!/bin/bash -eu

ROOT="${SRC:-$(cd "$(dirname "$0")/.." && pwd)}"
OUT_DIR="${OUT:?OUT must be set by ClusterFuzzLite}"
CXX_BIN="${CXX:-clang++}"
FUZZ_ENGINE="${LIB_FUZZING_ENGINE:-}"

COMMON_FLAGS=(
  -std=c++17
  -I"${ROOT}/include"
  -Wall
  -Wextra
  -Wpedantic
)

SOURCES=(
  "${ROOT}/src/analyzer.cc"
  "${ROOT}/src/bundle.cc"
  "${ROOT}/src/checksum.cc"
  "${ROOT}/src/profile.cc"
  "${ROOT}/src/reader.cc"
  "${ROOT}/src/route.cc"
  "${ROOT}/src/rules.cc"
  "${ROOT}/src/status.cc"
  "${ROOT}/src/stencil.cc"
  "${ROOT}/src/table.cc"
)

mkdir -p "${OUT_DIR}"

"${CXX_BIN}" ${CXXFLAGS:-} "${COMMON_FLAGS[@]}" "${SOURCES[@]}" \
  "${ROOT}/fuzz/rivetmesh_bundle_fuzzer.cc" ${FUZZ_ENGINE} \
  -o "${OUT_DIR}/rivetmesh_bundle_fuzzer"

"${CXX_BIN}" ${CXXFLAGS:-} "${COMMON_FLAGS[@]}" "${SOURCES[@]}" \
  "${ROOT}/fuzz/rivetmesh_rules_fuzzer.cc" ${FUZZ_ENGINE} \
  -o "${OUT_DIR}/rivetmesh_rules_fuzzer"
