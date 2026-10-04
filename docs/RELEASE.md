# Release process

The source tree is licensed for open-source release. Public hosting, tags,
GitHub releases and PyPI uploads are separate publisher actions.

Before tagging:

1. Build/install from a clean source archive with the documented dependencies.
2. Run native and Python tests plus the synthetic GUI smoke check.
3. Record environment and limitations in `VALIDATION.md`.
4. Confirm source archive includes LICENSE, NOTICE, docs and synthetic generator,
   and excludes room captures, caches, model weights and private paths/secrets.
5. Build the Python wheel and source distribution; test a fresh installation.
6. Review version numbers across CMake, package.xml and pyproject.toml.
   The public maintainer contacts are `solvorlabs@gmail.com` (primary) and
   `haarishuda090@gmail.com` (secondary).
7. Publish source under Solvor Labs when authorized. The Python package contains
   tools only; shared-library installation still follows the CMake instructions.

Do not label the 0.1 GUI renderer as a Gazebo sensor backend or physics model.
No ROS distribution release/bloom configuration or precompiled portable binary
is claimed by this source package.
