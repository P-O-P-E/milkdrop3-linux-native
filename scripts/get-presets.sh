#!/usr/bin/env bash
set -euo pipefail

if [[ "$(uname -s)" == "Darwin" ]]; then
    DATA_ROOT="${HOME}/Library/Application Support/MilkDrop3 Native"
else
    DATA_ROOT="${XDG_DATA_HOME:-${HOME}/.local/share}/milkdrop3-linux"
fi
PRESET_ROOT="${DATA_ROOT}/presets"
TEXTURE_ROOT="${DATA_ROOT}/textures"

mkdir -p "${DATA_ROOT}"

update_or_clone() {
    local repository="$1"
    local destination="$2"
    if [[ -d "${destination}/.git" ]]; then
        git -C "${destination}" pull --ff-only
    elif [[ -e "${destination}" ]]; then
        echo "Refusing to replace existing non-Git path: ${destination}" >&2
        exit 1
    else
        git clone --depth 1 "${repository}" "${destination}"
    fi
}

update_or_clone \
    https://github.com/projectM-visualizer/presets-cream-of-the-crop.git \
    "${PRESET_ROOT}"
update_or_clone \
    https://github.com/projectM-visualizer/presets-milkdrop-texture-pack.git \
    "${TEXTURE_ROOT}"

echo "Presets installed in ${PRESET_ROOT}"
echo "Textures installed in ${TEXTURE_ROOT}"
