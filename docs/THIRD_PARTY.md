# Provenance and external dependencies

The source originated in Solvor Labs' `swarm_platform` research working tree,
specifically `swarm_simulation/src/architect_gaussian_sim`. The renderer source
was untracked in that donor snapshot, so the donor HEAD alone does not identify
it. The release records source hashes in `docs/VALIDATION.md`.

The extraction keeps the project's own C++/GLSL renderer and replaces the
Architect cache, session loading, build paths and scene tooling. The published
spherical harmonic basis and covariance/Jacobian mathematics are implemented
in the shader. No gsplat CUDA implementation, Nerfstudio trainer, INRIA
rasterizer code, model weights or third-party captured dataset is bundled.

| Dependency | Use | Upstream license/source |
| --- | --- | --- |
| Gazebo GUI/rendering/transport/common/plugin/msgs | dynamic runtime libraries | [Gazebo source](https://github.com/gazebosim/gz-rendering), Apache-2.0 |
| Ogre-Next | dynamic scene/geometry/material API | [Ogre-Next](https://github.com/OGRECave/ogre-next), see its MIT license notices |
| Qt 5 Core/Quick | dynamic GUI/runtime | [Qt licensing](https://doc.qt.io/archives/qt-5.15/licensing.html), LGPLv3/GPL/commercial options depending on distribution |
| Python and standard library | conversion tools | [PSF license](https://docs.python.org/3/license.html) |

The Apache-2.0 license covers this project's source. It does not relicense
external libraries or imported user data. Source releases do not bundle those
libraries. If distributing linked binaries, retain the required dependency
notices and comply with the terms of the specific library distributions used.

Format reference: [gsplat exporter](https://github.com/nerfstudio-project/gsplat/blob/main/gsplat/exporter.py).
GUI plugin lookup: [Gazebo GUI plugins](https://gazebosim.org/api/gui/8/plugins.html).
