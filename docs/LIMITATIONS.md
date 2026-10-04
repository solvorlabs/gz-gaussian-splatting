# Scope and known limits

- The plugin injects static visual content into the GUI Ogre2 scene. Server
  sensor images, depth, LiDAR rays, physics and collision queries do not see it.
- Tested target: Linux/Ubuntu 24.04, Harmonic and Qt 5. Other platforms and
  Gazebo releases are not yet claimed.
- A single user-camera scene is the supported runtime. Multiple viewports,
  cross-scene sharing, hot reload/unloading and dynamic clouds are not release
  features. Restart Gazebo when changing a cache.
- PNG capture reads the GUI camera's completed render buffer. It does not
  capture the desktop or rescale pixels; dimensions follow the actual GUI
  viewport. A functioning GUI/OpenGL context remains necessary. Xvfb/Mesa CI
  configuration is provided; Wayland and headless server-only rendering are
  not validated release targets.
- The renderer evaluates degree-3 SH using signed 12-bit values in [-8,8].
  Values outside this range are clipped; `gzgs inspect` reports the count.
  Opacity is quantized to 8 bits. These choices are not lossless.
- Sorting uses 65,536 depth buckets and is throttled to 100 ms. It approximates
  per-Gaussian back-to-front order; near-equal depths and camera motion can
  differ from a CUDA tiled rasterizer. No per-pixel early termination is used.
- Maximum two million cache records. Geometry-shader expansion and blending
  may be fill-rate bound for wide translucent splats. Release builds matter.
- Gaussian near-plane centers are rejected and projected covariance receives
  a 0.3-pixel-squared low-pass variance. Antialiased/compensated gsplat variants
  need separate parity work.
- Shader colour assumes display-encoded training RGB and Ogre2's sRGB output.
  It is not a universal linear-radiance or HDR renderer.
- Training quality, pose accuracy, texture coverage and unseen-view stability
  remain properties of the reconstruction. Rendering cannot repair them.
