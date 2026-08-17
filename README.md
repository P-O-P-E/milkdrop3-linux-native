# MilkDrop3 Linux Native

A native C++20 MilkDrop-compatible visualizer for Debian and Ubuntu. It uses
[libprojectM 4](https://github.com/projectM-visualizer/projectm) for OpenGL rendering and SDL2 for windowing,
input, and audio capture. Wine is not used at build time or runtime.

> [!IMPORTANT]
> This is an independent compatibility project. It is not affiliated with or endorsed by the MilkDrop3,
> MilkDrop, Winamp, or projectM authors. The current release is the native `.milk` MVP; MilkDrop3-specific
> `.milk2`, mash-up, and MilkPanel compatibility are tracked in the [roadmap](docs/ROADMAP.md).

## Current capabilities

- Native x86-64 Linux executable using SDL2 and OpenGL
- Standard `.milk` and `.prjm` preset discovery and rendering
- PipeWire/PulseAudio/ALSA capture devices exposed through SDL2
- Selectable capture device, including monitor sources for desktop audio
- Smooth and beat-driven transitions through libprojectM
- Shuffle, ordered playback, preset history, and preset lock
- Resizable, HiDPI-aware window and desktop fullscreen mode
- Drag-and-drop preset files and directories
- Texture search paths and XDG-compatible configuration
- Mouse/touch waveform controls supported by projectM
- Automated core tests, Ubuntu CI, and Debian/TGZ packaging configuration

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

## Audio devices

List the capture devices SDL can see:

```bash
./build/release/milkdrop3-linux --list-audio-devices
```

Select one by index or by a unique part of its name:

```bash
./build/release/milkdrop3-linux --audio-device 2
./build/release/milkdrop3-linux --audio-device monitor
```

On PipeWire and PulseAudio systems, output-monitor sources may need to be selected after the program starts. Install
`pavucontrol`, open its **Recording** tab, and assign `milkdrop3-linux` to the desired output monitor.

## Controls

| Key/input | Action |
|---|---|
| `Space`, `Right`, `N` | Next preset with a smooth transition |
| `Left`, `Backspace`, `P` | Previous preset from history |
| `R` | Random next preset |
| `S` | Toggle shuffled/ordered playback |
| `L` | Lock or unlock automatic preset changes |
| `A` | Cycle audio capture devices |
| `[` / `]` | Decrease/increase beat sensitivity |
| `F`, `Alt+Enter` | Toggle desktop fullscreen |
| `Shift+left click` | Add a projectM touch waveform |
| Middle click | Clear touch waveforms |
| Mouse wheel | Change preset |
| Drag and drop | Add a `.milk` file or preset directory |
| `F1`, `H` | Print controls in the launching terminal |
| `Escape`, `Ctrl+Q` | Quit |

## Configuration

Copy the supplied configuration file:

```bash
mkdir -p ~/.config/milkdrop3-linux
cp config/config.ini ~/.config/milkdrop3-linux/config.ini
```

The application follows `XDG_CONFIG_HOME` and `XDG_DATA_HOME`. Command-line options override configuration-file
values. Run `milkdrop3-linux --help` for all options.

The default data paths are:

```text
~/.local/share/milkdrop3-linux/presets
~/.local/share/milkdrop3-linux/textures
```

## Manual build

Install or build libprojectM 4.1 or newer, then configure this project with its installation prefix:

```bash
sudo apt install build-essential cmake ninja-build libsdl2-dev libgl1-mesa-dev
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

## Project documentation

- [Architecture](docs/ARCHITECTURE.md)
- [Compatibility roadmap](docs/ROADMAP.md)
- [Third-party components and licenses](docs/THIRD_PARTY.md)
- [Contributing](CONTRIBUTING.md)
- [Security policy](SECURITY.md)

## License

The original code in this repository is MIT-licensed. libprojectM and preset packs retain their own licenses; see
[third-party notices](docs/THIRD_PARTY.md). No proprietary MilkDrop3 shaders, executables, or assets are included.

