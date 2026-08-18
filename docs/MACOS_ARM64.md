# Apple Silicon macOS guide

> [!WARNING]
> This branch is a pre-release development preview. Its `.app` and DMG are ad-hoc signed for local testing, are not
> notarized public releases, and do not yet represent full MilkDrop3 compatibility.

The `macos-arm64` branch produces a native Apple Silicon application. It intentionally rejects Intel and universal
build configurations so an accidental Rosetta dependency cannot enter the release. Supported hardware includes M1,
M2, M3, M4, and later `arm64` Macs. The deployment target is macOS 14.0 or newer so the bundled Homebrew runtime and
CI runner share a supported baseline.

## Requirements

- Apple Silicon Mac running macOS 14 or newer
- Xcode command-line tools: `xcode-select --install`
- Native Homebrew installed at `/opt/homebrew`
- Git, CMake, Ninja, and a C++20-capable AppleClang toolchain (the bootstrap script installs/checks these as appropriate)
- Internet access for Homebrew, Dear ImGui, and the libprojectM source checkout
- Free disk space for Homebrew packages, libprojectM sources, build trees, the application bundle, and DMG
- **Screen & System Audio Recording** permission when using the native system-playback source
- **Microphone** permission only when using a microphone or virtual input device

The build is native `arm64`; an Intel Homebrew installation under `/usr/local` cannot supply its dependencies. Install
or migrate to Apple Silicon Homebrew at `/opt/homebrew` before running the bootstrap script.

## Build and package

```bash
git clone --branch macos-arm64 https://github.com/P-O-P-E/milkdrop3-linux-native.git
cd milkdrop3-linux-native
./scripts/bootstrap-macos-arm64.sh
./scripts/get-presets.sh
```

The script performs these checks and actions:

1. Rejects non-Darwin and non-`arm64` hosts.
2. Rejects Intel Homebrew installations.
3. Installs Homebrew's native SDL2 compatibility runtime, SDL2_image, and build dependencies.
4. Builds libprojectM 4.1.7 and MilkDrop3 Native with `CMAKE_OSX_ARCHITECTURES=arm64`.
5. Runs dependency-free tests.
6. Creates `MilkDrop3 Native.app`, copies non-system dynamic libraries into `Contents/Frameworks`, and rewrites their
   install names.
7. Verifies every bundled Mach-O file contains arm64 code, signs the bundle, and produces a DMG.

Build products are written below `build/macos-arm64` and `build/macos-arm64-stage`.

## Audio source selection

Open the in-window preset browser with `Tab`, then use **Audio source** to choose between:

- **System audio (native macOS mix)** — captures the current Mac playback mix through ScreenCaptureKit.
- **Default capture device** or a named CoreAudio input — captures a microphone or virtual input through SDL.

The `A` key cycles the same sources. The command line can select the native system mix directly:

```bash
"build/macos-arm64-stage/MilkDrop3 Native.app/Contents/MacOS/MilkDrop3 Native" --audio-device system
```

On first selection, allow **MilkDrop3 Native** under **System Settings → Privacy & Security → Screen & System Audio
Recording**. If macOS asks you to quit and reopen the application after granting access, do so. ScreenCaptureKit captures
the system mix rather than a particular speaker/headphone endpoint; select the playback endpoint in macOS Sound settings.
The application excludes its own process audio, and macOS or protected-content policies may omit some streams.

When a microphone or virtual input is selected, allow the application under **Privacy & Security → Microphone**. The
application continues without live audio when the applicable permission is denied.

BlackHole or Loopback is no longer required for normal system-mix visualization. It remains useful for custom routing,
isolating one application, or feeding a specific virtual/multi-output path. After configuring such a device, select it
like any other named input:

```bash
"build/macos-arm64-stage/MilkDrop3 Native.app/Contents/MacOS/MilkDrop3 Native" --list-audio-devices
"build/macos-arm64-stage/MilkDrop3 Native.app/Contents/MacOS/MilkDrop3 Native" --audio-device BlackHole
```

## Data and presets

The native macOS defaults are:

```text
~/Library/Application Support/MilkDrop3 Native/config.ini
~/Library/Application Support/MilkDrop3 Native/presets
~/Library/Application Support/MilkDrop3 Native/textures
~/Library/Application Support/MilkDrop3 Native/generated
~/Library/Application Support/MilkDrop3 Native/library.db
```

Create the preset directory and copy compatible `.milk` presets into it, or drag a preset directory onto the running
window.

## Signing and notarization

Local builds are ad-hoc signed by default. They are suitable for development but are not Apple-notarized. Finder may
require **Control-click → Open** for a downloaded development build.

For redistribution, pass a Developer ID Application identity while building:

```bash
export CODESIGN_IDENTITY="Developer ID Application: Your Name (TEAMID)"
./scripts/bootstrap-macos-arm64.sh
```

Store notarization credentials once:

```bash
xcrun notarytool store-credentials milkdrop3-notary \
  --apple-id you@example.com \
  --team-id TEAMID \
  --password app-specific-password
```

Then notarize and staple the generated DMG:

```bash
NOTARY_PROFILE=milkdrop3-notary \
  ./scripts/notarize-macos.sh build/macos-arm64/milkdrop3-native-0.3.0-macos-arm64.dmg
```

Developer ID credentials are deliberately not stored in the repository or GitHub workflow.

## Graphics implementation

The macOS build requests a native OpenGL 4.1 core context and GLSL 4.10. Apple has deprecated OpenGL but continues to
provide the framework on supported macOS versions. No Wine, Rosetta, XQuartz, GTK, or Qt compatibility layer is used.
The renderer remains libprojectM, allowing the same `.milk` preset pipeline as Linux.
