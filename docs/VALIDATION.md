# Validation record — 0.1.0 baseline

Recorded 2026-10-04. This is a standalone extraction, not a claim that every
Gazebo distribution, GPU or renderer-parity configuration has been tested.

## Tested environment

- Ubuntu 24.04, GCC 13, C++17, Qt 5, ROS 2 Jazzy vendor dependency route.
- Gazebo Sim 8.15.0; GUI 8.4.0; rendering 8.2.3; transport 13.6.0;
  msgs 10.4.0; common 5.9.0; plugin 2.0.4; Ogre-Next 2.3.3.
- OpenGL 4.5, Mesa 25.2.8 llvmpipe (LLVM 20.1.2), desktop XCB.
- Python 3.10.11 and system Python 3.12.

## Local checks

| Check | Evidence/result |
| --- | --- |
| Standalone Release CMake build/install | C++ plugin compiled and installed without machine-specific dependency paths |
| Native C++ tests | Cache length/header, scale/quaternion validity, SH quantization bound |
| Python tests | 17 tests passed on both Python 3.10 and 3.12 |
| Cross-language ABI | Python-written cache parsed by compiled C++ probe; corrupt non-finite cache rejected |
| GUI small cloud | 2,078 original synthetic splats; correct amber/green walls, checkerboard floor and red anisotropic Gaussian |
| GUI large index path | 70,000 synthetic splats; 32-bit index upload and visible scene verified |
| PNG smoke gate | 1,199×952 actual camera pixels; all four synthetic colour features present |
| ROS optional integration | `WITH_AMENT=ON` built/installed; colcon build, ament discovery and GUI plugin path hook passed |
| Python distribution | Source archive and wheel built; fresh wheel installed outside source tree; CLI demo/inspect worked |
| Source archive core build | Archive extracted separately; no-Gazebo core build and both CTest groups passed |
| Source archive full build | Final source archive extracted independently; complete GUI plugin build and both CTest groups passed |
| In-place repository build | Separate repository built/installed; both CTest groups and GUI smoke gate passed from its actual location |
| Shell syntax | Build, demo and smoke scripts passed `bash -n` |

The GUI smoke checks use bounded `gz sim` runs. Exit 124 from `timeout` is
expected, not a renderer failure. `scripts/check_screenshot.py` rejects blank
images and missing coloured features; it does not measure PSNR/SSIM parity.

## Extraction fixes verified here

- Explicit geometry-stage `gl_PerVertex` input matching the vertex output.
  The inherited shader compiled individually but failed strict-driver linking.
- Correct handling of Ogre's uint16 indices for small scenes. The donor's large
  reconstructions used uint32; its assumed-width upload corrupted small clouds.
- Render-buffer PNG capture. Desktop capture could return black despite a
  visible rendered image, and the old implementation rescaled captured pixels.
- New cache magic, strict validation, portable dependency discovery, clean
  CLI/scene generation, and removal of Architect/session/Redis assumptions.

## Not yet validated / not supplied

- GitHub Actions jobs are configured but have not run on GitHub.
- Native OSRF apt route is documented, not tested separately from Jazzy vendors.
- Xvfb, NVIDIA/AMD hardware drivers, Wayland, other Gazebo releases and OSes.
- Two-million-splat performance/memory envelope and bit-exact gsplat parity.
- Live camera-pose following end-to-end; callback/basis implementation retained
  with finite-pose validation, but no standalone runtime replay test claimed.
- Sensor/depth/LiDAR integration, collision geometry, GUI hot reload, multiple
  viewports and server-only headless rendering are outside this release scope.

## Donor provenance and integrity

Source: `swarm_simulation/src/architect_gaussian_sim` in the Solvor Labs R&D
working tree. Branch: `backend`; HEAD:
`c754344792f4e285e2d5f3494c8f7f17c8366425`. Source was untracked, so these hashes
identify the working-tree code actually extracted:

| Donor file | SHA-256 |
| --- | --- |
| `src/ArchitectGaussianRenderer.cc` | `06ba69f788dac105ef78427ac49deec7f55be4437a743f12acf15fb77cf31f3d` |
| `src/ArchitectGaussianRenderer.hh` | `ce6d3adfb8733027f44c5a86e6ebbb8d61c23c08204e8d3d3999d3a715fb9cc7` |
| `src/ArchitectGaussianRenderer.qml` | `2a1e160fcc3527a43b889b5b51d1bab1cd2fcc283e1c29f2e5b4551fb119022c` |
| `src/ArchitectGaussianRenderer.qrc` | `8893c17886452cbdff048bbf1dc505c24f874beea93bbe7b9a72af10d4b2f3d6` |

Donor branch, HEAD and porcelain-status fingerprint remained unchanged during
extraction. Status fingerprint (SHA-256 of `git status --porcelain=v1 -z`):
`d091938848b8abb074f24e79f727d0a5b452b0c43b7199760b3125dd6fa3dd4a`.
No donor code, configuration, Git history, model weights or private captured
room data was changed or migrated. No commits or public publication were made.
