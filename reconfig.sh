#!/bin/bash
BASE_DIR="$(cd "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
DEFAULT_PROFILE="${BASE_DIR}/profiles/macos-x86_64"
PROFILE_PATH="${1:-${DEFAULT_PROFILE}}"
MODE_ARG="${2:-}"
COVERAGE="OFF"

if [[ "${1:-}" == "coverage" ]]; then
  PROFILE_PATH="${BASE_DIR}/profiles/macos-x86_64-debug"
  MODE_ARG="coverage"
fi

# Always keep outputs under build/, with a build type subfolder.
case "${PROFILE_PATH}" in
  *debug*) BUILD_TYPE="Debug" ; MODE="debug" ;;
  *) BUILD_TYPE="Release" ; MODE="release" ;;
esac

OUTPUT_DIR="${BASE_DIR}/build/${MODE_ARG:-${MODE}}"

usage() {
  echo "Usage: $0 [profile_path] [release|debug|coverage]"
  echo ""
  echo "Examples:"
  echo "  $0                               # Release build under build/release"
  echo "  $0 ./profiles/macos-x86_64-debug # Debug build under build/debug"
  echo "  $0 ./profiles/macos-x86_64 debug # Force debug output under build/debug"
  echo "  $0 coverage                     # Build tests and generate coverage"
}

if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
  usage
  exit 0
fi

if [[ "${PROFILE_PATH}" != /* ]]; then
  PROFILE_PATH="${BASE_DIR}/${PROFILE_PATH}"
fi

if [[ -n "${MODE_ARG}" ]]; then
  case "${MODE_ARG}" in
    debug) OUTPUT_DIR="${BASE_DIR}/build/debug" ;;
    release) OUTPUT_DIR="${BASE_DIR}/build/release" ;;
    coverage) OUTPUT_DIR="${BASE_DIR}/build/coverage"; BUILD_TYPE="Debug"; COVERAGE="ON" ;;
    *) OUTPUT_DIR="${BASE_DIR}/build/${2}" ;;
  esac
fi

# 2. Clear the target output folder and regenerate dependencies.
mkdir -p "${OUTPUT_DIR}"
rm -rf "${OUTPUT_DIR}"/*

conan install . \
  --profile:host="${PROFILE_PATH}" \
  --profile:build="${PROFILE_PATH}" \
  --output-folder="${OUTPUT_DIR}" \
  --build=missing

# 3. Reconfigure the project using the Conan-generated toolchain.
cmake -S "${BASE_DIR}" -B "${OUTPUT_DIR}" \
  -DCMAKE_TOOLCHAIN_FILE="${OUTPUT_DIR}/conan_toolchain.cmake" \
  -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
  -DENABLE_COVERAGE="${COVERAGE}"

# 4. Compile the binary executable.
if [[ "${MODE_ARG}" == "coverage" ]]; then
  cmake --build "${OUTPUT_DIR}" --target unit_tests
  cmake --build "${OUTPUT_DIR}" --target coverage
else
  cmake --build "${OUTPUT_DIR}"
fi

