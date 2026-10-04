# Contributing

Maintainer: Solvor Labs — primary `solvorlabs@gmail.com`, secondary
`haarishuda090@gmail.com`.

Use a separate build directory and include a reproducer for rendering bugs.
Report Gazebo, Ogre, Qt, GPU/driver versions, cache record count, camera mode,
and whether the issue also appears with `gzgs demo`. Do not upload private
captures or model artifacts without permission to redistribute them.

Run `ctest --test-dir build --output-on-failure`. Converter/cache changes must
preserve the Python/C++ ABI, finite-input validation, no-clobber output behavior,
SH layout and quaternion conventions. Rendering changes need an actual
synthetic-scene smoke check as well as compile/test evidence.

Source contributions use Apache-2.0 and retain existing copyright notices.
Keep PRs focused; new platform or rendering-engine support must include build
and runtime validation for that target.
