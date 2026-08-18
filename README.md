# MilkDrop3 Native

A native C++20 MilkDrop-compatible visualizer for Debian, Ubuntu, and Apple Silicon macOS. It uses
[libprojectM 4](https://github.com/projectM-visualizer/projectm) for OpenGL rendering and SDL2 for windowing,
input, and audio capture. Wine is not used on Linux, and the macOS build is native `arm64` without Rosetta.

> [!WARNING]
> **Pre-release software:** this is a development preview, not a stable or notarized public release. Expect missing
> MilkDrop3 features, compatibility gaps, and occasional build/runtime defects. Keep original presets backed up and
> report reproducible problems through [GitHub Issues](https://github.com/P-O-P-E/milkdrop3-linux-native/issues).

> [!IMPORTANT]
> This is an independent compatibility project. It is not affiliated with or endorsed by the MilkDrop3,
> MilkDrop, Winamp, or projectM authors. The current release supports native `.milk` playback and portable
> MilkDrop 2-style authoring/mashups. MilkDrop3-specific `.milk2` and MilkPanel compatibility remain on the
> [roadmap](docs/ROADMAP.md).

## Current capabilities

- Native x86-64 Linux executable and native Apple Silicon `arm64` application bundle
- Standard `.milk` and `.prjm` preset discovery and rendering
- PipeWire/PulseAudio/ALSA and macOS CoreAudio input devices exposed through SDL2
- Selectable audio source in the in-window UI, including Linux monitor sources and macOS virtual audio inputs
- Native Apple Silicon system-playback capture through ScreenCaptureKit (no virtual loopback driver required)
- Smooth, eased fade-out/fade-in transitions for manual, timed, and beat-driven preset changes
- Shuffle, ordered playback, preset history, and preset lock
- Resizable, HiDPI-aware window and desktop fullscreen mode
- Drag-and-drop preset files and directories
- Texture search paths with XDG/Linux and Application Support/macOS configuration
- Mouse/touch waveform controls supported by projectM
- In-window preset browser with search, favorites, ratings, and play counts
- Rating/favorite-weighted shuffle and M3U playlist import/export
- Raw preset editor with syntax diagnostics, search/replace, preview, and non-destructive save copies
- Mashups for general effects, motion/equations, waves, shapes, warp shaders, and composite shaders
- Wave/shape fragment export and reuse as mashup donors
- Generated-preset rename and confirmed, recoverable move-to-trash; imported presets remain read-only
- Timed or persistent PNG/JPEG/WebP overlays, including image drag-and-drop
- On-screen preset, lock, rating, favorite, play-count, and error/status display
- Automated core tests, Ubuntu CI, and Debian/TGZ packaging configuration
- M1-hosted macOS CI, dependency-bundled `.app`, ad-hoc signing, and DMG packaging

## Prerequisites

| Platform | Required before building |
|---|---|
| Debian/Ubuntu | 64-bit Linux, `sudo`/APT access, Git, internet access, a working OpenGL 3.3 driver, and enough space to build libprojectM. `bootstrap-debian.sh` installs the compiler, CMake, Ninja, SDL2, SDL2_image, GLM, and Mesa development packages. |
| Apple Silicon macOS | M1 or later, macOS 14+, Xcode Command Line Tools, Git/internet access, native ARM Homebrew at `/opt/homebrew`, and an OpenGL 4.1-capable system. The first system-audio selection also requires Screen & System Audio Recording permission. |

Presets are not bundled automatically with the source checkout; run `scripts/get-presets.sh` or provide your own
compatible `.milk` preset directory. This pre-release has automated Ubuntu and M1 build coverage, but its full hardware,
driver, preset, signing, and distribution test matrix is not yet complete.

## Quick start on Apple Silicon macOS

The `macos-arm64` branch targets M1, M2, M3, M4, and later Apple Silicon Macs running macOS 14 or newer:

Prerequisites are an Apple Silicon Mac, macOS 14+, Xcode Command Line Tools, internet access, and a native ARM
Homebrew installation at `/opt/homebrew` (an Intel `/usr/local` Homebrew installation is not sufficient).

```bash
xcode-select --install
git clone --branch macos-arm64 https://github.com/P-O-P-E/milkdrop3-linux-native.git
cd milkdrop3-linux-native
./scripts/bootstrap-macos-arm64.sh
./scripts/get-presets.sh
open "build/macos-arm64-stage/MilkDrop3 Native.app"
```

The bootstrap script requires native Homebrew under `/opt/homebrew`, builds libprojectM and the application as `arm64`,
runs the tests, bundles non-system dynamic libraries, signs the bundle, and creates a DMG. See the
[Apple Silicon guide](docs/MACOS_ARM64.md) for the full prerequisite list, audio permissions/routing, Gatekeeper,
Developer ID signing, and notarization.

## Quick start on Ubuntu or Debian

The bootstrap script installs build dependencies, builds libprojectM 4.1.7 locally, builds this application, and runs
the tests:

```bash
git clone https://github.com/P-O-P-E/milkdrop3-linux-native.git
cd milkdrop3-linux-native
./scripts/bootstrap-debian.sh
./scripts/get-presets.sh
./build/release/milkdrop3-linux
```

After the first run, use `pavucontrol` or your desktop audio settings to select a monitor source if the default input is
a microphone.

## Audio sources

List the available sources:

```bash
./build/release/milkdrop3-linux --list-audio-devices
```

Select one by index or by a unique part of its name:

```bash
./build/release/milkdrop3-linux --audio-device 2
./build/release/milkdrop3-linux --audio-device monitor
```

The in-window **Audio source** menu provides the same choices. Press `A` to cycle them without opening the menu.

On PipeWire and PulseAudio systems, output-monitor sources may need to be selected after the program starts. Install
`pavucontrol`, open its **Recording** tab, and assign `milkdrop3-linux` to the desired output monitor.

On Apple Silicon macOS, select **System audio (native macOS mix)** in the menu or launch with
`--audio-device system`. macOS will request **Screen & System Audio Recording** permission the first time. This captures
the Mac's current playback mix; use macOS Sound settings to choose whether that mix plays through speakers, headphones,
or another output. Microphones and virtual inputs remain available as separate choices.

## Controls

| Key/input | Action |
|---|---|
| `Space`, `Right`, `N` | Next preset with a smooth transition |
| `Left`, `Backspace`, `P` | Previous preset from history |
| `R` | Random next preset |
| `S` | Toggle shuffled/ordered playback |
| `L` | Lock or unlock automatic preset changes |
| `A` | Cycle audio sources (system playback, microphone, and virtual/monitor inputs) |
| `[` / `]` | Decrease/increase beat sensitivity |
| `F`, `Alt+Enter` | Toggle desktop fullscreen |
| `Shift+left click` | Add a projectM touch waveform |
| Middle click | Clear touch waveforms |
| Mouse wheel | Change preset |
| Drag and drop | Add a `.milk` file or preset directory |
| Drag an image | Add a timed image overlay |
| `Tab` | Toggle the in-window preset browser |
| `F1`, `H` | Print controls in the launching terminal |
| `Escape`, `Ctrl+Q`, `Command+Q` | Quit |

## Preset transitions

Runtime preset changes now fade the current visualization to black, load the next preset while fully covered, and then
fade the new visualization in. The opacity follows a smoothstep curve so the beginning, midpoint, and end do not snap.
This also softens beat-triggered changes that projectM would otherwise request as hard cuts.

`fade_duration` is the total time for both halves of the transition and defaults to `2.4` seconds. For a slower fade:

```ini
fade_duration=4.0
```

Set `fade_duration=0` to disable the application fade and return to projectM's native transition behavior. In that mode,
`transition_duration` controls native smooth transitions and beat-triggered hard cuts can be abrupt.

## Configuration

Copy the supplied configuration file:

```bash
mkdir -p ~/.config/milkdrop3-linux
cp config/config.ini ~/.config/milkdrop3-linux/config.ini
```

On Linux, the application follows `XDG_CONFIG_HOME` and `XDG_DATA_HOME`. On macOS it uses
`~/Library/Application Support/MilkDrop3 Native`. Command-line options override configuration-file values. Ratings,
favorites, and play counts are stored in `library.db`; edited and mashup presets are saved under the generated preset
directory. The editor never overwrites the source preset. Files removed through the browser are moved to
`generated/.trash` and are not loaded during preset scans.

The default data paths are:

```text
~/.local/share/milkdrop3-linux/presets
~/.local/share/milkdrop3-linux/textures
~/.local/share/milkdrop3-linux/generated
```

On macOS, presets, textures, metadata, generated files, and UI state live below:

```text
~/Library/Application Support/MilkDrop3 Native
```

## Manual build

Install or build libprojectM 4.1 or newer, then configure this project with its installation prefix:

```bash
sudo apt install build-essential cmake ninja-build libsdl2-dev libsdl2-image-dev libgl1-mesa-dev
cmake -S . -B build/release -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH=/path/to/projectm/install
cmake --build build/release --parallel
ctest --test-dir build/release --output-on-failure
```

Core parsing and catalog tests do not require SDL, OpenGL, or projectM:

```bash
cmake --preset core-tests
cmake --build --preset core-tests
ctest --preset core-tests
```

## Packages

After a release build, create a Debian package and a portable source/binary archive with:

```bash
cd build/release
cpack -G DEB
cpack -G TGZ
```

Until libprojectM 4 is broadly available from distribution repositories, the bootstrap-built executable uses an RPATH
to its locally installed projectM library. Release packaging will bundle the required library after the first public
compatibility milestone.

On Apple Silicon, `./scripts/bootstrap-macos-arm64.sh` produces an arm64-only `.app` and DMG. The unsigned developer
build is ad-hoc signed and remains a pre-release artifact; public redistribution should use a Developer ID identity and
Apple notarization as described in the macOS guide.

## Project documentation

- [Architecture](docs/ARCHITECTURE.md)
- [Compatibility roadmap](docs/ROADMAP.md)
- [MilkDrop 2.25c compatibility reference](docs/MILKDROP2_REFERENCE.md)
- [Apple Silicon macOS build and runtime guide](docs/MACOS_ARM64.md)
- [Third-party components and licenses](docs/THIRD_PARTY.md)
- [Contributing](CONTRIBUTING.md)
- [Security policy](SECURITY.md)

## License

The original code in this repository is MIT-licensed. libprojectM and preset packs retain their own licenses; see
[third-party notices](docs/THIRD_PARTY.md). No proprietary MilkDrop3 shaders, executables, or assets are included.
