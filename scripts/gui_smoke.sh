#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 Solvor Labs
set -euo pipefail
project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
test_dir="$(mktemp -d -t gzgs-smoke-XXXXXXXX)"
export PYTHONPATH="$project_dir/python${PYTHONPATH:+:$PYTHONPATH}"
export GZ_GUI_PLUGIN_PATH="${GZGS_PLUGIN_DIR:-$project_dir/install/lib}${GZ_GUI_PLUGIN_PATH:+:$GZ_GUI_PLUGIN_PATH}"
export GZ_PARTITION="gzgs-smoke-$$"
python_bin="${PYTHON_BIN:-python3}"
"$python_bin" -m gz_gaussian_splatting.cli demo --output-dir "$test_dir"
"$python_bin" -m gz_gaussian_splatting.cli world "$test_dir/room.gzgs" "$test_dir/smoke.sdf" \
  --mode captured --camera-pose 6 -6 4 0 .35 2.35 --screenshot "$test_dir/scene.png"
set +e
timeout -s INT 20s gz sim -r -v 3 "$test_dir/smoke.sdf" > "$test_dir/gazebo.log" 2>&1
result=$?
set -e
if [[ "$result" != 0 && "$result" != 124 ]]; then
  echo "Gazebo failed ($result). Inspect $test_dir/gazebo.log" >&2
  exit "$result"
fi
"$python_bin" "$project_dir/scripts/check_screenshot.py" "$test_dir/scene.png"
echo "Smoke evidence retained at $test_dir"
