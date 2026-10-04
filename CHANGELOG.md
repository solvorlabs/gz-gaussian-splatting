# Changelog

## 0.1.0 — unreleased

- Extracted the native Ogre2 Gaussian renderer from the Solvor Labs R&D project.
- Added standalone CMake and optional ament/colcon builds.
- Added strict versioned cache and streaming PLY conversion for SH degrees 0–3.
- Added generic overview, diagnostic and calibrated captured-camera worlds.
- Added original synthetic scene, tests, CI and Apache-2.0 release documentation.
- Fixed strict-driver GLSL stage-interface linking and small-cloud uint16
  index-buffer uploads inherited from the original large-cloud renderer.
- PNG screenshots read the completed GUI camera buffer instead of the desktop,
  without pixel rescaling or Qt window access from the render thread.
