#!/usr/bin/env bash
set -euo pipefail

if [[ "$(uname -s)" != "Darwin" ]]; then
    echo "Notarization must run on macOS with Xcode command-line tools." >&2
    exit 1
fi
if [[ $# -ne 1 || ! -f "$1" ]]; then
    echo "Usage: NOTARY_PROFILE=profile-name $0 path/to/milkdrop3-native.dmg" >&2
    exit 1
fi

NOTARY_PROFILE="${NOTARY_PROFILE:-milkdrop3-notary}"
DISK_IMAGE="$(cd -- "$(dirname -- "$1")" && pwd)/$(basename -- "$1")"

xcrun notarytool submit "${DISK_IMAGE}" --keychain-profile "${NOTARY_PROFILE}" --wait
xcrun stapler staple "${DISK_IMAGE}"
xcrun stapler validate "${DISK_IMAGE}"

echo "Notarized and stapled: ${DISK_IMAGE}"
