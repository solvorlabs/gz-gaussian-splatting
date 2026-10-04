# Build and install

The supported baseline is Ubuntu 24.04, Gazebo Harmonic/Ogre2, Qt 5 and GCC 13.
Gazebo Classic and Ogre1 are not supported. Use either dependency route below;
avoid mixing native OSRF and ROS vendor binaries within one build.

## ROS 2 Jazzy vendor route

On a machine already configured with the official ROS 2 apt repository:

```bash
sudo apt install build-essential cmake python3-venv qtbase5-dev qtdeclarative5-dev \
  ros-jazzy-ros-gz ros-jazzy-gz-common-vendor ros-jazzy-gz-gui-vendor \
  ros-jazzy-gz-plugin-vendor ros-jazzy-gz-rendering-vendor \
  ros-jazzy-gz-msgs-vendor ros-jazzy-gz-transport-vendor ros-jazzy-gz-ogre-next-vendor
source /opt/ros/jazzy/setup.bash
```

## Native Gazebo route

Configure the official [Harmonic apt repository](https://gazebosim.org/docs/harmonic/install_ubuntu/)
first, then install:

```bash
sudo apt install build-essential cmake python3-venv qtbase5-dev qtdeclarative5-dev \
  gz-harmonic libgz-common5-dev libgz-gui8-dev libgz-plugin2-dev \
  libgz-rendering8-dev libgz-msgs10-dev libgz-transport13-dev libogre-next-dev
```

## Build

```bash
python3 -m venv .venv
.venv/bin/pip install .
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$PWD/install"
cmake --build build -j2
ctest --test-dir build --output-on-failure
cmake --install build
export GZ_GUI_PLUGIN_PATH="$PWD/install/lib${GZ_GUI_PLUGIN_PATH:+:$GZ_GUI_PLUGIN_PATH}"
```

Use `-DCMAKE_INSTALL_LIBDIR=lib` if a custom toolchain chooses a different
library directory. `GZ_GUI_PLUGIN_PATH` must point to the actual installed
shared-library directory. Installation is relocatable; runtime dependency
paths come from the Gazebo environment rather than this repository.

For a source installation, add its prefixes to `CMAKE_PREFIX_PATH`. CMake cache
variables `OGRE_NEXT_INCLUDE_DIR`, `OGRE_NEXT_MAIN_LIBRARY`, and
`GZ_RENDERING_OGRE2_LIBRARY` can override auto-discovery. Match the Ogre ABI
used by gz-rendering8; do not point to a different Ogre version.

## Colcon

The same repository may be placed in a ROS workspace `src/` directory:

```bash
colcon build --packages-select gz_gaussian_splatting \
  --cmake-args -DWITH_AMENT=ON -DCMAKE_BUILD_TYPE=Release
source install/setup.bash
```

The Python CLI remains a separately installed package. The GUI plugin uses
Gazebo Transport; it does not require a ROS node or DDS topic. The ament
environment hook adds the installed library directory to `GZ_GUI_PLUGIN_PATH`.

## Conversion-only/native-core tests

```bash
cmake -S . -B build-core -DBUILD_RENDERER=OFF
cmake --build build-core
ctest --test-dir build-core --output-on-failure
```

These checks run without Gazebo, Qt, ROS, CUDA or external Python packages.
