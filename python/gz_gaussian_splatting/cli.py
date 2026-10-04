"""SPDX-License-Identifier: Apache-2.0 — Copyright 2026 Solvor Labs."""
from __future__ import annotations
import argparse
import json
import math
from pathlib import Path
import shutil
import subprocess
import sys
from .cache import MAX_RECORDS, read_cache, write_cache
from .demo import demo_gaussians
from .ply import load_ply
from .scene import write_world


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(prog="gzgs", description="Gazebo Gaussian-splat scene tools")
    subs = parser.add_subparsers(dest="command", required=True)
    convert = subs.add_parser("convert", help="Convert standard 3DGS or RGB PLY to .gzgs")
    convert.add_argument("input", type=Path)
    convert.add_argument("output", type=Path)
    convert.add_argument("--force", action="store_true")
    convert.add_argument("--min-opacity", type=float, default=0.0)
    convert.add_argument("--max-points", type=int, default=MAX_RECORDS)
    convert.add_argument("--point-radius", type=float)
    convert.add_argument("--scale", type=float, default=1.0)
    convert.add_argument("--translate", nargs=3, type=float, default=(0, 0, 0))
    inspect = subs.add_parser("inspect", help="Validate and summarize a .gzgs cache")
    inspect.add_argument("input", type=Path)
    demo = subs.add_parser("demo", help="Generate a redistributable synthetic room")
    demo.add_argument("--output-dir", type=Path, default=Path("demo-output"))
    demo.add_argument("--force", action="store_true")
    world = subs.add_parser("world", help="Create a Gazebo SDF world around an existing cache")
    world.add_argument("input", type=Path)
    world.add_argument("output", type=Path)
    world.add_argument("--mode", choices=("overview", "diagnostic", "captured"), default="overview")
    world.add_argument("--camera-pose", nargs=6, type=float, metavar=("X", "Y", "Z", "ROLL", "PITCH", "YAW"))
    world.add_argument("--intrinsics", nargs=6, type=float, metavar=("W", "H", "FX", "FY", "CX", "CY"))
    world.add_argument("--pose-topic")
    world.add_argument("--camera-target")
    world.add_argument("--screenshot", type=Path)
    world.add_argument("--force", action="store_true")
    subs.add_parser("doctor", help="Report Gazebo and CLI prerequisites without modifying the system")
    args = parser.parse_args(argv)
    try:
        if args.command == "convert":
            if args.input.resolve() == args.output.resolve():
                raise ValueError("input and output must differ")
            count = write_cache(args.output, load_ply(args.input,
                min_opacity=args.min_opacity, max_points=args.max_points,
                point_radius=args.point_radius, scale=args.scale,
                translate=tuple(args.translate)), force=args.force)
            print(json.dumps({"gaussians": count, "cache": str(args.output.resolve())}))
        elif args.command == "inspect":
            minimum, maximum = [math.inf]*3, [-math.inf]*3
            count = clipped = 0
            for point in read_cache(args.input):
                count += 1
                clipped += sum(abs(v) > 8 for v in point.sh)
                for axis, value in enumerate(point.position):
                    minimum[axis] = min(minimum[axis], value)
                    maximum[axis] = max(maximum[axis], value)
            print(json.dumps({"format": "GZGS0001", "gaussians": count, "minimum": minimum,
                              "maximum": maximum, "sh_values_clipped_by_gpu_packing": clipped}))
        elif args.command == "demo":
            cache = args.output_dir / "room.gzgs"
            world_path = args.output_dir / "room.sdf"
            if not args.force and (cache.exists() or world_path.exists()):
                raise FileExistsError("demo output exists; select a new directory or use --force")
            write_cache(cache, demo_gaussians(), force=args.force)
            write_world(cache, world_path, force=args.force)
            print(world_path.resolve())
        elif args.command == "world":
            if args.input.resolve() == args.output.resolve():
                raise ValueError("input and output must differ")
            write_world(args.input, args.output, mode=args.mode,
                camera_pose=tuple(args.camera_pose) if args.camera_pose else None,
                intrinsics=tuple(args.intrinsics) if args.intrinsics else None,
                pose_topic=args.pose_topic, camera_target=args.camera_target,
                screenshot=args.screenshot, force=args.force)
            print(args.output.resolve())
        elif args.command == "doctor":
            gz = shutil.which("gz")
            print(json.dumps({"python": sys.version.split()[0], "gz": gz,
                              "cmake": shutil.which("cmake"), "display": __import__("os").environ.get("DISPLAY")}))
            if gz:
                subprocess.run([gz, "sim", "--versions"], check=False)
    except (OSError, ValueError, OverflowError, UnicodeError, KeyError) as exc:
        print(f"gzgs: {exc}", file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
