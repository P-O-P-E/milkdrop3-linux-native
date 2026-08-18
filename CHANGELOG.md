# Changelog

All notable changes are documented here. This project follows semantic versioning after the initial compatibility
releases.

## Unreleased — pre-release

- Replaced abrupt runtime preset cuts with a configurable, eased OpenGL fade-out/load/fade-in transition.
- Added native Apple Silicon system-playback capture through ScreenCaptureKit alongside SDL microphone/virtual inputs.
- Added an in-window audio-source selector and `--audio-device system` support.
- Documented pre-release status, build prerequisites, audio permissions, and macOS output-routing behavior.

## 0.3.0 — 2026-08-17

- Bundled Homebrew's dynamically loaded SDL3 runtime on macOS and added a packaged-executable smoke test.
- Added an arm64-only Apple Silicon build with an M1 GitHub Actions runner.
- Added a native macOS OpenGL 4.1 path, SDL2 main integration, and Apple-native application data directories.
- Added a macOS `.app`, Info.plist microphone declaration, entitlement, icon, dependency fixup, signing, and DMG packaging.
- Added Homebrew/libprojectM bootstrap and optional Developer ID notarization workflows.
- Documented macOS audio permission and virtual system-audio routing.

## 0.2.0 — 2026-08-17

- Added a native Dear ImGui preset browser, search, status overlay, ratings, favorites, and play counts.
- Added persistent library metadata, rating/favorite-weighted shuffle, and M3U playlist import/export.
- Added a loss-aware MilkDrop preset document parser with duplicate, malformed-line, and numbered-code-gap diagnostics.
- Added non-destructive authoring, in-memory preview, generated save copies, and libprojectM compile errors.
- Added six-part MilkDrop 2-style mashups and wave/shape fragment export.
- Added generated-preset rename and confirmed, recoverable move-to-trash workflows.
- Added timed and persistent image overlays through SDL2_image and OpenGL.
- Added compatibility documentation and expanded dependency-free tests.

## 0.1.0 — 2026-08-17

- Initial native Debian/Ubuntu application.
- Added SDL2/OpenGL lifecycle and libprojectM 4 integration.
- Added selectable audio capture, preset discovery/history, transitions, fullscreen, drag-and-drop, and mouse controls.
- Added XDG configuration, core tests, CI, bootstrap/preset scripts, desktop integration, and packaging scaffolding.
