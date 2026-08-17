# Contributing

Contributions are welcome, particularly testing on Debian/Ubuntu, audio-device handling, preset compatibility reports,
authoring workflows, and `.milk2` format documentation.

## Development checks

Run the dependency-free tests before submitting changes:

```bash
cmake --preset core-tests
cmake --build --preset core-tests
ctest --preset core-tests
```

For a complete build, run `./scripts/bootstrap-debian.sh`. Keep platform APIs behind the existing component boundaries,
use RAII for owned SDL/projectM resources, and do not add proprietary MilkDrop3 binaries, shaders, or assets.

## Commit and pull-request scope

- Keep changes focused and explain user-visible behavior.
- Include dependency-free tests for configuration, catalogs, preset documents, mashups, or library behavior.
- Document new command-line or configuration settings.
- Report the Debian/Ubuntu release, desktop audio stack, GPU, and driver when filing rendering defects.
