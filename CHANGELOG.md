# Changelog

All notable changes are documented here. This project follows semantic versioning after the initial compatibility
releases.

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
