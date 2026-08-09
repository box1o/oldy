#!/usr/bin/env bash
set -euo pipefail

cc="${1:?C compiler}"
cxx="${2:?C++ compiler}"
use_ccache="${3:-ON}"

prefix_path="${DAWN_PREFIX}"
if [[ "$(uname -s)" == "Darwin" && -n "${CMAKE_PREFIX_PATH:-}" ]]; then
  prefix_path="${CMAKE_PREFIX_PATH};${prefix_path}"
fi

common_flags=(
  -B build -S .
  -G Ninja
  -DCMAKE_BUILD_TYPE=Release
  -DCMAKE_POLICY_VERSION_MINIMUM=3.5
  -DCMAKE_MESSAGE_LOG_LEVEL=NOTICE
  -DBUILD_TESTING=ON
  -DCMAKE_C_COMPILER="${cc}"
  -DCMAKE_CXX_COMPILER="${cxx}"
  -DCMAKE_PREFIX_PATH="${prefix_path}"
  -DDawn_DIR="${Dawn_DIR}"
)

if [[ "${use_ccache}" == "ON" ]]; then
  common_flags+=(
    -DCMAKE_C_COMPILER_LAUNCHER=ccache
    -DCMAKE_CXX_COMPILER_LAUNCHER=ccache
  )
fi

cmake "${common_flags[@]}"

cmake --build build --target woki_tests woki_extensions wokiext studio -j"$(sysctl -n hw.ncpu 2>/dev/null || nproc)"

if [[ "$(uname -s)" == "Darwin" && -n "${WOKI_LLVM_PREFIX:-}" ]]; then
  export DYLD_LIBRARY_PATH="${WOKI_LLVM_PREFIX}/lib:${DYLD_LIBRARY_PATH:-}"
fi

ctest --test-dir build --output-on-failure

if [[ "${use_ccache}" == "ON" ]]; then
  ccache -s
fi
