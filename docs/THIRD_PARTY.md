# Third-party notices

This repository does not vendor third-party source, presets, textures, or binaries. The build and download scripts fetch
the following independent projects at the user's request. macOS packages copy the resulting dynamic libraries into the
application bundle so the DMG does not depend on the end user's Homebrew installation.

| Component | Purpose | License |
|---|---|---|
| [libprojectM](https://github.com/projectM-visualizer/projectm) | MilkDrop preset parsing, FFT/beat analysis, and OpenGL rendering | LGPL-2.1-or-later |
| [SDL2](https://github.com/libsdl-org/SDL) / [sdl2-compat](https://github.com/libsdl-org/sdl2-compat) | Windowing, input, OpenGL context, and audio capture | Zlib |
| [SDL3](https://github.com/libsdl-org/SDL) | Runtime used behind Homebrew's macOS sdl2-compat package | Zlib |
| [SDL2_image](https://github.com/libsdl-org/SDL_image) | PNG/JPEG/WebP image overlays | Zlib |
| [Dear ImGui](https://github.com/ocornut/imgui) | Native in-window browser and authoring interface | MIT |
| [Cream of the Crop](https://github.com/projectM-visualizer/presets-cream-of-the-crop) | Optional preset collection | Per-preset/repository notices |
| [MilkDrop texture pack](https://github.com/projectM-visualizer/presets-milkdrop-texture-pack) | Optional textures used by community presets | Repository notices |

The MilkDrop3 name identifies the compatibility goal and upstream file formats. The project does not include the
upstream Windows executable, installer, proprietary shaders, artwork, or preset distribution.

Linux uses distribution-provided OpenGL. macOS uses Apple's system OpenGL framework, `iconutil`, code-signing tools,
and optional notarization tools supplied with macOS/Xcode; those system components are not redistributed.
