#!/usr/bin/env bash
#
# build.sh — configure and build the NewCsio project on Linux.
#
# Usage:
#   ./build.sh                 # configure (if needed) and build (Release)
#   ./build.sh -t Debug        # build with Debug configuration
#   ./build.sh -c              # clean the build directory first
#   ./build.sh -j 4            # limit parallel jobs to 4
#   ./build.sh -b out          # use "out" as the build directory
#   ./build.sh -h              # show help

set -euo pipefail

# --- defaults ---------------------------------------------------------------
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_TYPE="Release"
BUILD_DIR="${SCRIPT_DIR}/build"
CLEAN=0
JOBS="$(nproc 2>/dev/null || echo 2)"

# --- help -------------------------------------------------------------------
usage() {
    sed -n '2,13p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'
    exit "${1:-0}"
}

# --- parse args -------------------------------------------------------------
while getopts ":t:b:j:ch" opt; do
    case "${opt}" in
        t) BUILD_TYPE="${OPTARG}" ;;
        b) BUILD_DIR="${OPTARG}" ;;
        j) JOBS="${OPTARG}" ;;
        c) CLEAN=1 ;;
        h) usage 0 ;;
        :) echo "Error: -${OPTARG} requires an argument." >&2; usage 1 ;;
        \?) echo "Error: unknown option -${OPTARG}." >&2; usage 1 ;;
    esac
done

# --- dependency checks ------------------------------------------------------
check_deps() {
    local missing=0

    if ! command -v cmake >/dev/null 2>&1; then
        echo "Error: cmake is not installed (need >= 3.14)." >&2
        echo "  Debian/Ubuntu: sudo apt-get install cmake" >&2
        missing=1
    fi

    if ! command -v pkg-config >/dev/null 2>&1; then
        echo "Error: pkg-config is not installed." >&2
        echo "  Debian/Ubuntu: sudo apt-get install pkg-config" >&2
        missing=1
    fi

    if command -v pkg-config >/dev/null 2>&1 && \
       ! pkg-config --exists gstreamer-1.0; then
        echo "Error: GStreamer 1.0 development files not found." >&2
        echo "  Debian/Ubuntu: sudo apt-get install libgstreamer1.0-dev" >&2
        missing=1
    fi

    if ! command -v cc >/dev/null 2>&1 && ! command -v gcc >/dev/null 2>&1; then
        echo "Error: no C/C++ compiler found." >&2
        echo "  Debian/Ubuntu: sudo apt-get install build-essential" >&2
        missing=1
    fi

    [ "${missing}" -eq 0 ] || exit 1
}

# --- build ------------------------------------------------------------------
check_deps

if [ "${CLEAN}" -eq 1 ] && [ -d "${BUILD_DIR}" ]; then
    echo ">> Cleaning ${BUILD_DIR}"
    rm -rf "${BUILD_DIR}"
fi

echo ">> Configuring (${BUILD_TYPE}) in ${BUILD_DIR}"
cmake -S "${SCRIPT_DIR}" -B "${BUILD_DIR}" \
    -DCMAKE_BUILD_TYPE="${BUILD_TYPE}"

echo ">> Building with ${JOBS} job(s)"
cmake --build "${BUILD_DIR}" --parallel "${JOBS}"

echo ">> Done. Artifacts are in ${BUILD_DIR}"
echo "   Run the demo with: ${BUILD_DIR}/newcsio_demo"
