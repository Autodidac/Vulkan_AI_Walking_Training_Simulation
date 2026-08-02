#!/usr/bin/env bash
set -euo pipefail

if [[ -z "${VCPKG_ROOT:-}" && -f "${HOME}/vcpkg/scripts/buildsystems/vcpkg.cmake" ]]; then
    export VCPKG_ROOT="${HOME}/vcpkg"
fi

: "${VCPKG_ROOT:?VCPKG_ROOT is not set and vcpkg was not found at ${HOME}/vcpkg}"

if [[ ! -f "${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" ]]; then
    echo "Invalid VCPKG_ROOT: ${VCPKG_ROOT}" >&2
    exit 1
fi

if [[ ! -x "${VCPKG_ROOT}/vcpkg" ]]; then
    "${VCPKG_ROOT}/bootstrap-vcpkg.sh" -disableMetrics
fi

echo "Configuring with vcpkg manifest mode..."
cmake --preset linux-release --fresh
cmake --build --preset linux-release
ctest --test-dir build/linux-release --output-on-failure

echo "Built: build/linux-release/Runner"
