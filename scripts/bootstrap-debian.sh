#!/usr/bin/env bash
set -euo pipefail

PROJECT_ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_ROOT="${PROJECT_ROOT}/build"
DEPENDENCY_ROOT="${BUILD_ROOT}/dependencies"
PROJECTM_SOURCE="${DEPENDENCY_ROOT}/projectm-src"
PROJECTM_BUILD="${DEPENDENCY_ROOT}/projectm-build"
PROJECTM_PREFIX="${DEPENDENCY_ROOT}/projectm-install"
APP_BUILD="${BUILD_ROOT}/release"
PROJECTM_VERSION="v4.1.7"

if [[ "${1:-}" != "--skip-packages" ]]; then
    sudo apt-get update
    sudo apt-get install -y \
        build-essential cmake git ninja-build pkg-config \
        libgl1-mesa-dev mesa-common-dev libsdl2-dev libglm-dev
fi

mkdir -p "${DEPENDENCY_ROOT}"

if [[ ! -d "${PROJECTM_SOURCE}/.git" ]]; then
    git clone --branch "${PROJECTM_VERSION}" --depth 1 --recurse-submodules \
        https://github.com/projectM-visualizer/projectm.git "${PROJECTM_SOURCE}"
else
    git -C "${PROJECTM_SOURCE}" fetch --tags --force
    git -C "${PROJECTM_SOURCE}" checkout "${PROJECTM_VERSION}"
    git -C "${PROJECTM_SOURCE}" submodule update --init --recursive
fi

cmake -S "${PROJECTM_SOURCE}" -B "${PROJECTM_BUILD}" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="${PROJECTM_PREFIX}" \
    -DBUILD_SHARED_LIBS=ON \
    -DBUILD_TESTING=OFF \
    -DENABLE_INSTALL=ON \
    -DENABLE_PLAYLIST=OFF \
    -DENABLE_SDL_UI=OFF \
    -DENABLE_SYSTEM_PROJECTM_EVAL=OFF
cmake --build "${PROJECTM_BUILD}" --parallel
cmake --install "${PROJECTM_BUILD}"

cmake -S "${PROJECT_ROOT}" -B "${APP_BUILD}" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH="${PROJECTM_PREFIX}" \
    -DCMAKE_INSTALL_RPATH="${PROJECTM_PREFIX}/lib" \
    -DBUILD_TESTING=ON
cmake --build "${APP_BUILD}" --parallel
ctest --test-dir "${APP_BUILD}" --output-on-failure

echo
echo "Build complete. Run:"
echo "  ${APP_BUILD}/milkdrop3-linux"
echo
echo "Install presets with:"
echo "  ${PROJECT_ROOT}/scripts/get-presets.sh"

