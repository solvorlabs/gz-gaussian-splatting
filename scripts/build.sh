#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 Solvor Labs
set -euo pipefail
project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$project_dir"
python_bin="${PYTHON_BIN:-python3}"
"$python_bin" -m venv .venv
.venv/bin/python -m pip install .
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$project_dir/install" -DCMAKE_INSTALL_LIBDIR=lib \
  -DPython3_EXECUTABLE="$(command -v "$python_bin")"
cmake --build build -j "${BUILD_JOBS:-2}"
ctest --test-dir build --output-on-failure
cmake --install build
printf 'Built successfully. Run: %s/scripts/run_demo.sh\n' "$project_dir"
