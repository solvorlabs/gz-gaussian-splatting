"""SPDX-License-Identifier: Apache-2.0 — Copyright 2026 Solvor Labs.

Streaming standard 3DGS/Splatfacto PLY import; no training stack dependency.
"""
from __future__ import annotations
from dataclasses import dataclass
import math
from pathlib import Path
import struct
from typing import BinaryIO, Iterator
from .cache import Gaussian, MAX_RECORDS, SH_C0, rgb_sh

TYPES = {"char": "b", "int8": "b", "uchar": "B", "uint8": "B",
         "short": "h", "int16": "h", "ushort": "H", "uint16": "H",
         "int": "i", "int32": "i", "uint": "I", "uint32": "I",
         "float": "f", "float32": "f", "double": "d", "float64": "d"}


@dataclass(frozen=True)
class PlyHeader:
    count: int
    format: str
    properties: tuple[tuple[str, str], ...]


def read_header(handle: BinaryIO) -> PlyHeader:
    if handle.readline(8193) not in (b"ply\n", b"ply\r\n"):
        raise ValueError("input is not a PLY file")
    properties: list[tuple[str, str]] = []
    fmt = ""
    count = 0
    element = ""
    seen_vertex = False
    total = 0
    while True:
        raw = handle.readline(8193)
        total += len(raw)
        if not raw or len(raw) > 8192 or total > 65536:
            raise ValueError("truncated or excessive PLY header")
        fields = raw.decode("ascii").split()
        if not fields or fields[0] in {"comment", "obj_info"}:
            continue
        if fields[0] == "end_header":
            break
        if fields[0] == "format":
            if len(fields) != 3 or fields[2] != "1.0":
                raise ValueError("PLY format must use version 1.0")
            fmt = fields[1]
        elif fields[0] == "element":
            if len(fields) != 3:
                raise ValueError("malformed PLY element")
            element = fields[1]
            size = int(fields[2])
            if size < 0:
                raise ValueError("negative PLY element count")
            if not seen_vertex and element != "vertex" and size:
                raise ValueError("vertex must be the first non-empty PLY element")
            if element == "vertex":
                if seen_vertex:
                    raise ValueError("duplicate vertex element")
                seen_vertex = True
                count = size
        elif fields[0] == "property" and element == "vertex":
            if len(fields) != 3 or fields[1] not in TYPES:
                raise ValueError("vertex list/unsupported properties are not supported")
            if fields[2] in {name for name, _ in properties}:
                raise ValueError("duplicate vertex property")
            properties.append((fields[2], fields[1]))
    if fmt not in {"ascii", "binary_little_endian", "binary_big_endian"}:
        raise ValueError(f"unsupported PLY format: {fmt}")
    if not 1 <= count <= 20_000_000 or not properties:
        raise ValueError("PLY vertex count must be in [1, 20000000]")
    return PlyHeader(count, fmt, tuple(properties))


def vertex_rows(handle: BinaryIO, header: PlyHeader) -> Iterator[dict[str, float]]:
    names = tuple(name for name, _ in header.properties)
    record = struct.Struct((">" if header.format == "binary_big_endian" else "<") +
                           "".join(TYPES[kind] for _, kind in header.properties))
    for index in range(header.count):
        if header.format == "ascii":
            raw = handle.readline(65537)
            fields = raw.split()
            if len(raw) > 65536 or len(fields) != len(names):
                raise ValueError(f"malformed/truncated PLY vertex {index}")
            values = tuple(float(value) for value in fields)
        else:
            raw = handle.read(record.size)
            if len(raw) != record.size:
                raise ValueError(f"truncated PLY vertex {index}")
            values = record.unpack(raw)
        if not all(math.isfinite(v) for v in values):
            raise ValueError(f"non-finite PLY vertex {index}")
        yield dict(zip(names, values))


def _gaussian(row: dict[str, float], *, point_radius: float | None,
              scale: float, translate: tuple[float, float, float]) -> Gaussian:
    position = tuple(row[axis] * scale + translate[i] for i, axis in enumerate("xyz"))
    if any(f"scale_{i}" in row for i in range(3)) and not all(f"scale_{i}" in row for i in range(3)):
        raise ValueError("incomplete Gaussian scale fields")
    if any(f"f_dc_{i}" in row for i in range(3)) and not all(f"f_dc_{i}" in row for i in range(3)):
        raise ValueError("incomplete Gaussian DC colour fields")
    if all(f"scale_{i}" in row for i in range(3)):
        radii = tuple(math.exp(row[f"scale_{i}"]) * scale for i in range(3))
        if not all(f"rot_{i}" in row for i in range(4)) or "opacity" not in row:
            raise ValueError("3DGS input requires rot_0..3 and opacity")
        w, x, y, z = (row[f"rot_{i}"] for i in range(4))
        norm = math.sqrt(w*w + x*x + y*y + z*z)
        if norm < 1e-6:
            raise ValueError("zero PLY quaternion")
        quaternion = (x/norm, y/norm, z/norm, w/norm)
        logit = row["opacity"]
        opacity = 1/(1+math.exp(-logit)) if logit >= 0 else math.exp(logit)/(1+math.exp(logit))
    elif point_radius is not None:
        radii = (point_radius * scale,) * 3
        quaternion = (0.0, 0.0, 0.0, 1.0)
        opacity = 1.0
    else:
        raise ValueError("missing Gaussian scales; for an RGB point cloud supply --point-radius")
    rest_names = sorted((name for name in row if name.startswith("f_rest_")),
                        key=lambda name: int(name[7:]))
    if all(f"f_dc_{i}" in row for i in range(3)):
        dc = tuple(row[f"f_dc_{i}"] for i in range(3))
        coefficients = len(rest_names) // 3
        if len(rest_names) not in (0, 9, 24, 45) or rest_names != [f"f_rest_{i}" for i in range(len(rest_names))]:
            raise ValueError("PLY SH must have contiguous degree 0, 1, 2 or 3 coefficients")
        rest = tuple(row[f"f_rest_{channel * coefficients + coefficient}"]
                     for coefficient in range(coefficients) for channel in range(3))
        sh = dc + rest + (0.0,) * (45 - len(rest))
        rgb = tuple(max(0, min(255, round((0.5 + SH_C0 * value) * 255))) for value in dc)
    elif all(name in row for name in ("red", "green", "blue")):
        if rest_names:
            raise ValueError("SH rest values require f_dc_0..2")
        rgb = tuple(round(row[name]) for name in ("red", "green", "blue"))
        if any(not 0 <= value <= 255 for value in rgb):
            raise ValueError("RGB input must use values in [0,255]")
        sh = rgb_sh(rgb)
    else:
        raise ValueError("PLY requires f_dc_0..2 or red/green/blue")
    return Gaussian(position, radii, quaternion, sh, rgb + (round(opacity * 255),))


def load_ply(path: Path, *, min_opacity: float = 0.0, max_points: int = MAX_RECORDS,
             point_radius: float | None = None, scale: float = 1.0,
             translate: tuple[float, float, float] = (0.0, 0.0, 0.0)) -> Iterator[Gaussian]:
    if not 0 <= min_opacity <= 1 or not 1 <= max_points <= MAX_RECORDS:
        raise ValueError("invalid opacity/point limit")
    if (not math.isfinite(scale) or scale <= 0 or len(translate) != 3 or
            not all(math.isfinite(v) for v in translate)):
        raise ValueError("scale must be positive and transform finite")
    if point_radius is not None and (not math.isfinite(point_radius) or point_radius <= 0):
        raise ValueError("point radius must be positive")
    with Path(path).open("rb") as handle:
        header = read_header(handle)
        if header.count > max_points and max_points == MAX_RECORDS:
            raise ValueError("PLY exceeds the default capacity; explicitly set --max-points to downsample")
        stride = max(1, math.ceil(header.count / max_points))
        for index, row in enumerate(vertex_rows(handle, header)):
            # Validate every row, including filtered ones, so a bad export is
            # never silently presented as a faithful reconstruction.
            gaussian = _gaussian(row, point_radius=point_radius, scale=scale, translate=translate)
            gaussian.validate()
            if index % stride == 0 and gaussian.rgba[3] / 255 >= min_opacity:
                yield gaussian
