# MilkDrop 2.25c compatibility reference

The open MilkDrop 2.25c source was reviewed as a behavioral reference for this project. Its main implementation is
BSD-3-Clause licensed, but this repository does not copy or compile that legacy source. The native implementation uses
independently structured C++20 modules and libprojectM's public API.

## Adopted behavior

| MilkDrop 2 state domain | Native implementation |
|---|---|
| General/post-processing | `PresetPart::General` |
| Motion and per-frame/per-pixel equations | `PresetPart::Motion` |
| Built-in/custom waves and motion vectors | `PresetPart::Waves` |
| Custom shapes | `PresetPart::Shapes` |
| Warp pixel shader | `PresetPart::WarpShader` |
| Composite pixel shader | `PresetPart::CompositeShader` |

`MashupEngine` can replace any combination of these domains in a base preset. The result is serialized as a normal
`.milk` document and previewed through `projectm_load_preset_data()`. Warp/composite mashups normalize the MilkDrop 2
version header so shader-specific versions remain explicit.

The authoring workflow also incorporates the useful high-level behavior of the historical preset browser, ratings,
save-as workflow, shape/wave import/export, and compile-error display. Source presets are read-only from the editor's
perspective; all writes create new files in the generated preset directory.

## Deliberately excluded

- Winamp and Wasabi plug-in interfaces
- Win32 configuration/resource dialogs and desktop-window embedding
- Direct3D 9 device, texture, font, and shader objects
- Bundled DirectX libraries and old Visual Studio projects
- The bundled legacy NS-EEL implementation and platform-specific assembler
- Fixed-size shader buffers, global plug-in state, and unsafe C string handling

libprojectM already supplies a maintained OpenGL renderer, projectM-Eval integration, MilkDrop FFT/audio analysis,
custom waves/shapes, blur passes, texture sampling, HLSL-to-GLSL preset translation, and native transition shaders.

## Shader boundary

The reviewed source archive references `data/include.fx`, warp/composite vertex and pixel shaders, and blur shaders,
but those files are not present in the archive. Consequently it is not a shader pack and does not add redistributable
visual effects. Preset-embedded shader text is handled by libprojectM's HLSL parser and GLSL generator.

## Compatibility tests

Dependency-free tests cover:

- Case-insensitive key lookup while preserving original key text
- Unknown/comment/section preservation
- Duplicate-key and numbered-code-gap diagnostics
- Selective mashup domain replacement
- Modern shader-version header generation
- Ratings, favorites, play counts, and weighted selection metadata
- Playlist import/export and explicit preset selection
- Generated-directory ownership enforcement and recoverable trash exclusion

Future golden-image tests will compare rendered output against known-compatible presets without redistributing upstream
binaries or proprietary shader assets.
