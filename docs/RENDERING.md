# Renderer internals

`GaussianSplatRenderer` is a Gazebo GUI plugin, not a server system. It waits
for an initialized Ogre2 scene on the GUI `PreRender` event and injects one
Ogre `ManualObject`. Scene units and axes are passed through unchanged.

1. `ReadCache` validates the entire standalone cache before GPU allocation.
2. Each Gaussian becomes one point vertex. Texture-coordinate attributes pack
   48 SH coefficients, scale, opacity and a canonical quaternion.
3. The vertex shader evaluates degree-3 RGB SH for the camera-to-center
   direction, clamps display RGB and converts it to linear colour for Ogre's
   sRGB output. SH pairs are packed into exact 24-bit float integers using two
   signed 12-bit codes. RGB bytes in the cache are diagnostic, not shader colour.
4. The geometry shader computes `R diag(scale²) Rᵀ`, rotates covariance to
   camera space, applies a quotient-rule perspective Jacobian and a
   0.3-pixel² low-pass. Eigenvectors of projected covariance produce a four-vertex
   elliptical quad. Centers within 0.01 of the camera plane are rejected.
5. The fragment shader evaluates Gaussian opacity; alpha below 1/255 is
   discarded. Native Gazebo geometry can depth-occlude splats. Splats themselves
   do not write depth or contribute geometry to sensors/collisions.
6. CPU sorting updates the point index buffer far-to-near, using 65,536 buckets.
   Movement/rotation thresholds and a 100 ms minimum interval limit sorting.
   Both Ogre-selected 16-bit and 32-bit index buffers are supported.
7. Optional camera following is received by a Gazebo Transport callback and
   staged under a mutex; actual camera changes occur on `PreRender`.
8. Optional PNG capture copies the GUI camera's last completed render target
   on `Render`, after ten warm-up frames. It neither renders another frame nor
   reads/changes Qt window objects on the render thread.

Shader stage interfaces explicitly match, including `gl_PerVertex`, for
strict OpenGL linkers. Program names are versioned to prevent stale Ogre
microcode from mixing shader interfaces after upgrades. Compiler failures are
logged; the GUI smoke gate catches blank or missing-feature output as well.

Resource ownership follows the Ogre scene lifetime. Hot unloading and scene
replacement are not supported; restart Gazebo to change a cloud. Numerical
parity with gsplat and scalability to the two-million-record ceiling are not
assumed from a successful smoke test. See `LIMITATIONS.md` and `VALIDATION.md`.
