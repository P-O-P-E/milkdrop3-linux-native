# Compatibility roadmap

The public MilkDrop3 repository contains a Windows/Direct3D source snapshot from 2023, while its current 3.33 feature
set is not fully published. Newer behavior is therefore implemented from public documentation and compatible file
formats, without copying proprietary binaries or shaders.

## Milestone 0.1 — Native `.milk` MVP

- [x] SDL2/OpenGL native application lifecycle
- [x] libprojectM 4 renderer integration
- [x] Capture-device selection and monitor-source support through SDL
- [x] Recursive preset and texture discovery
- [x] Transitions, hard cuts, shuffle, history, and locking
- [x] Fullscreen, HiDPI resizing, mouse controls, and drag-and-drop
- [x] XDG configuration, tests, CI, and packaging scaffolding

## Milestone 0.2 — Desktop integration

- [ ] In-window settings and preset browser
- [ ] Direct PipeWire output-node selection
- [ ] Playlist import/export and favorites
- [ ] On-screen preset/status display
- [ ] AppImage with bundled libprojectM
- [ ] Debian package tested on clean Debian 12 and Ubuntu 24.04 installations

## Milestone 0.3 — `.milk2` double presets

- [ ] Parse and validate the documented `.milk2` container format
- [ ] Render two preset instances into off-screen textures
- [ ] Implement zoom, side, plasma, circle, checkerboard, curtain, line, and geometric blend patterns
- [ ] Persist blend pattern, direction, progress, and randomized parameters
- [ ] Smooth transitions into and out of double presets
- [ ] Golden-image compatibility test collection

## Milestone 0.4 — Mash-up and authoring

- [ ] Warp/composite mash-up controls
- [ ] Shape/wave import and export
- [ ] Expanded variable compatibility where supported by the execution engine
- [ ] Shader editor with compile diagnostics, search/replace, undo, and redo
- [ ] Preset save, rename, delete, and favorites workflows

## Milestone 1.0 — Production compatibility release

- [ ] Large preset corpus validation
- [ ] AMD, Intel, and NVIDIA Mesa/proprietary-driver test matrix
- [ ] Performance, shader-cache, and startup optimizations
- [ ] Accessibility and keyboard-only UI review
- [ ] Stable `.deb` and AppImage releases

## Known boundary

MilkDrop3 3.33 documents a small number of non-open-source shaders. They will not be redistributed. Compatible open
presets can be used, and equivalent clean-room presets may be developed separately when licensing permits.

