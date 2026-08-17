#!/usr/bin/env bash
set -euo pipefail

PROJECT_ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_ROOT="${PROJECT_ROOT}/build"
DEPENDENCY_ROOT="${BUILD_ROOT}/dependencies-macos-arm64"
PROJECTM_SOURCE="${DEPENDENCY_ROOT}/projectm-src"
PROJECTM_BUILD="${DEPENDENCY_ROOT}/projectm-build"
PROJECTM_PREFIX="${DEPENDENCY_ROOT}/projectm-install"
APP_BUILD="${BUILD_ROOT}/macos-arm64"
STAGE_ROOT="${BUILD_ROOT}/macos-arm64-stage"
PROJECTM_VERSION="v4.1.7"
DEPLOYMENT_TARGET="14.0"

if [[ "$(uname -s)" != "Darwin" ]]; then
    echo "This script must run on macOS." >&2
    exit 1
fi
if [[ "$(uname -m)" != "arm64" ]]; then
    echo "Native Apple Silicon is required; Rosetta/x86_64 builds are intentionally rejected." >&2
    exit 1
fi
if ! command -v brew >/dev/null 2>&1; then
    echo "Homebrew is required. Install the native arm64 Homebrew distribution first." >&2
    exit 1
fi

BREW_PREFIX="$(brew --prefix)"
if [[ "${BREW_PREFIX}" != "/opt/homebrew" ]]; then
    echo "Expected native Apple Silicon Homebrew at /opt/homebrew; found ${BREW_PREFIX}." >&2
    exit 1
fi

if [[ "${1:-}" != "--skip-packages" ]]; then
    brew install cmake ninja pkgconf sdl2-compat sdl2_image glm
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
    -DCMAKE_PREFIX_PATH="${BREW_PREFIX}" \
    -DCMAKE_OSX_ARCHITECTURES=arm64 \
    -DCMAKE_OSX_DEPLOYMENT_TARGET="${DEPLOYMENT_TARGET}" \
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
    -DCMAKE_PREFIX_PATH="${PROJECTM_PREFIX};${BREW_PREFIX}" \
    -DCMAKE_INSTALL_PREFIX="${STAGE_ROOT}" \
    -DCMAKE_OSX_ARCHITECTURES=arm64 \
    -DCMAKE_OSX_DEPLOYMENT_TARGET="${DEPLOYMENT_TARGET}" \
    -DMILKDROP3_PROJECTM_LICENSE="${PROJECTM_SOURCE}/COPYING" \
    -DMILKDROP3_WARNINGS_AS_ERRORS=ON \
    -DBUILD_TESTING=ON
cmake --build "${APP_BUILD}" --parallel
ctest --test-dir "${APP_BUILD}" --output-on-failure
cmake --install "${APP_BUILD}"

APP_BUNDLE="${STAGE_ROOT}/MilkDrop3 Native.app"
APP_EXECUTABLE="${APP_BUNDLE}/Contents/MacOS/MilkDrop3 Native"
if [[ "$(lipo -archs "${APP_EXECUTABLE}")" != "arm64" ]]; then
    echo "Application executable is not a native arm64-only binary." >&2
    lipo -archs "${APP_EXECUTABLE}" >&2
    exit 1
fi

MACHO_DIRECTORIES=("${APP_BUNDLE}/Contents/MacOS")
if [[ -d "${APP_BUNDLE}/Contents/Frameworks" ]]; then
    MACHO_DIRECTORIES+=("${APP_BUNDLE}/Contents/Frameworks")
fi
while IFS= read -r -d '' item; do
    if file "${item}" | grep -q "Mach-O"; then
        lipo -verify_arch arm64 "${item}"
    fi
done < <(find "${MACHO_DIRECTORIES[@]}" -type f -print0)

codesign --verify --deep --strict --verbose=2 "${APP_BUNDLE}"

(
    cd "${APP_BUILD}"
    cpack -G DragNDrop
)

echo
echo "Native Apple Silicon build complete:"
echo "  ${APP_BUNDLE}"
echo
echo "Disk image:"
find "${APP_BUILD}" -maxdepth 1 -name 'milkdrop3-native-*-macos-arm64.dmg' -print
