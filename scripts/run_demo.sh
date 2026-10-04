#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 Solvor Labs
set -euo pipefail
project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
if [[ ! -f "$project_dir/install/lib/libGaussianSplatRenderer.so" ]]; then
  echo 'Build first using scripts/build.sh (see docs/BUILD.md).' >&2
  exit 1
fi
if [[ ! -x "$project_dir/.venv/bin/gzgs" ]]; then
  echo 'Install the CLI first: .venv/bin/pip install .' >&2
  exit 1
fi
"$project_dir/.venv/bin/gzgs" demo --output-dir "$project_dir/demo-output" --force
export GZ_GUI_PLUGIN_PATH="$project_dir/install/lib${GZ_GUI_PLUGIN_PATH:+:$GZ_GUI_PLUGIN_PATH}"
exec gz sim -r "$project_dir/demo-output/room.sdf" "$@"
