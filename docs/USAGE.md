# Using the renderer

## Supported inputs

`gzgs convert input.ply scene.gzgs` accepts ASCII, little-endian binary, and
big-endian binary PLY 1.0 with a scalar vertex schema. The vertex element must
be the first non-empty element. Compressed PLY, `.splat` archives and list
properties are not supported.

Standard Gaussian exports contain `x/y/z`, log-standard-deviation
`scale_0..2`, `rot_0..3` in **wxyz** order, logit `opacity`, and either SH colour
or byte RGB. The converter normalizes quaternions, exponentiates scales,
applies sigmoid opacity, and interleaves channel-major PLY SH coefficients into
coefficient-major cache layout. SH degrees 0, 1, 2 and 3 are accepted; missing
higher degrees are zero-filled. Malformed, non-finite or truncated input fails
the conversion, and an incomplete cache is never published.

An ordinary RGB point cloud requires explicit `--point-radius 0.02`. This
assigns spherical Gaussians; it does not recover learned covariance from points.

Options:

```bash
gzgs convert input.ply scene.gzgs --min-opacity 0.02 --max-points 200000
gzgs convert input.ply scene.gzgs --scale 2 --translate 0 0 1
```

`--max-points` applies deterministic stride sampling before the opacity filter.
It is a display tradeoff, not a quality-preserving reconstruction operation.
Full clouds up to two million records are retained by default. No automatic
outlier clipping or bounds cropping is applied. Existing output requires
`--force`. `--scale` is a positive uniform scale applied to centers and radii.
Units and orientation remain the producer's responsibility.

## Interactive modes

```bash
gzgs world scene.gzgs scene.sdf --mode overview
gzgs world scene.gzgs diagnostic.sdf --mode diagnostic
```

Overview is an oblique free-camera view. Diagnostic adds a native reference
floor; it is not measured reconstructed geometry. Gazebo's view control plugin
provides orbit, pan and zoom.

## Captured camera

`--camera-pose` is `x y z roll pitch yaw` in **Gazebo camera axes**: +X forward,
+Y left, +Z up. Cloud points are passed through without axis conversion.

```bash
gzgs world scene.gzgs captured.sdf --mode captured \
  --camera-pose 6 -6 4 0 0.35 2.35 \
  --intrinsics 800 600 600 600 400 300 \
  --screenshot captured.png
```

Intrinsics order is width, height, fx, fy, cx, cy. This configures pinhole
projection on the user camera. A GUI viewport can be resized independently;
screenshots copy the completed Ogre camera render target, preserving actual
pixels and dimensions. No desktop capture or image stretching is used. Exact
reference-image evaluation still requires matching viewport shape/intrinsics
and checking the saved dimensions: Gazebo can resize the GUI camera independently
of the requested calibration. This is not a separate fixed-resolution sensor.
`--screenshot` also works in overview/diagnostic mode without intrinsics.

Optional camera following consumes `gz.msgs.Pose_V`:

```bash
gzgs world scene.gzgs playback.sdf --mode captured \
  --camera-pose 0 0 0 0 0 0 \
  --pose-topic /my/camera/poses --camera-target camera_frame
```

The matching pose name's quaternion is an **OpenGL camera-to-world** pose:
local -Z forward, +X right, +Y up. The plugin applies the fixed basis conversion
to Gazebo camera axes. Translation is in the same cloud coordinates. The
Gazebo quaternion basis is `(w,x,y,z)=(0.5,-0.5,0.5,0.5)`. Before the first pose
message, the initial SDF camera pose is used. This API does not assume ROS or
an Architect playback process.

## Embed in your own world

Add this to the world's `<gui>` containing an Ogre2 `MinimalScene` and
`GzSceneManager`, then export `GZ_GUI_PLUGIN_PATH`:

```xml
<plugin filename="GaussianSplatRenderer" name="Gaussian splats">
  <gz-gui>
    <property type="bool" key="visible">false</property>
    <property type="bool" key="showTitleBar">false</property>
    <property type="string" key="state">floating</property>
  </gz-gui>
  <cache_path>/absolute/path/scene.gzgs</cache_path>
  <radius_sigma>3.0</radius_sigma>
  <view_mode>overview</view_mode>
</plugin>
```

`radius_sigma` may be 1–6; default 3. A smaller value truncates Gaussian tails.
Relative cache paths resolve against the launch working directory. The world
generator writes absolute paths, so regenerate it when moving a scene.
