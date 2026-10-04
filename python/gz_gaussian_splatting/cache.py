"""SPDX-License-Identifier: Apache-2.0 — Copyright 2026 Solvor Labs.

Validated, little-endian Gaussian cache ABI shared with the C++ renderer.
"""
from __future__ import annotations
from dataclasses import dataclass
import math
import os
from pathlib import Path
import struct
import tempfile
from typing import Iterable, Iterator

MAGIC = b"GZGS0001"
HEADER = struct.Struct("<8sI")
RECORD = struct.Struct("<58f4B")
MAX_RECORDS = 2_000_000
SH_C0 = 0.28209479177387814


@dataclass(frozen=True)
class Gaussian:
    position: tuple[float, float, float]
    scale: tuple[float, float, float]
    quaternion_xyzw: tuple[float, float, float, float]
    sh: tuple[float, ...]
    rgba: tuple[int, int, int, int]

    def validate(self) -> None:
        if len(self.position) != 3 or len(self.scale) != 3 or len(self.quaternion_xyzw) != 4:
            raise ValueError("position/scale/quaternion dimensions must be 3/3/4")
        if len(self.sh) != 48 or len(self.rgba) != 4:
            raise ValueError("Gaussian requires 48 SH values and 4 colour bytes")
        values = self.position + self.scale + self.quaternion_xyzw + self.sh
        if not all(math.isfinite(v) and abs(v) <= 3.402823466e38 for v in values):
            raise ValueError("Gaussian has non-finite or non-float32 data")
        if min(self.scale) <= 0 or sum(v * v for v in self.quaternion_xyzw) < 1e-12:
            raise ValueError("Gaussian scales must be positive and quaternion nonzero")
        if min(struct.unpack("<3f", struct.pack("<3f", *self.scale))) <= 0:
            raise ValueError("Gaussian scale underflows float32")
        if any(not isinstance(v, int) or not 0 <= v <= 255 for v in self.rgba):
            raise ValueError("RGBA values must be bytes")


def rgb_sh(rgb: tuple[int, int, int]) -> tuple[float, ...]:
    return tuple((value / 255.0 - 0.5) / SH_C0 for value in rgb) + (0.0,) * 45


def write_cache(path: Path, gaussians: Iterable[Gaussian], *, force: bool = False) -> int:
    path = Path(path)
    if path.exists() and not force:
        raise FileExistsError(f"output exists (use --force): {path}")
    path.parent.mkdir(parents=True, exist_ok=True)
    count = 0
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(dir=path.parent, delete=False) as handle:
            temporary = Path(handle.name)
            handle.write(HEADER.pack(MAGIC, 0))
            for gaussian in gaussians:
                gaussian.validate()
                count += 1
                if count > MAX_RECORDS:
                    raise ValueError(f"cache exceeds {MAX_RECORDS} Gaussians")
                handle.write(RECORD.pack(*gaussian.position, *gaussian.scale,
                                        *gaussian.quaternion_xyzw, *gaussian.sh, *gaussian.rgba))
            if count == 0:
                raise ValueError("no Gaussians remain after conversion/filtering")
            handle.seek(0)
            handle.write(HEADER.pack(MAGIC, count))
        if force:
            os.replace(temporary, path)
        else:
            # Atomic no-clobber publish even if another writer appeared.
            os.link(temporary, path)
            temporary.unlink()
        temporary = None
    finally:
        if temporary is not None:
            temporary.unlink(missing_ok=True)
    return count


def read_cache(path: Path) -> Iterator[Gaussian]:
    with Path(path).open("rb") as handle:
        raw = handle.read(HEADER.size)
        if len(raw) != HEADER.size:
            raise ValueError("truncated Gaussian cache header")
        magic, count = HEADER.unpack(raw)
        if magic != MAGIC or not 1 <= count <= MAX_RECORDS:
            raise ValueError("invalid Gaussian cache magic/count")
        if os.fstat(handle.fileno()).st_size != HEADER.size + count * RECORD.size:
            raise ValueError("cache length does not match its record count")
        for _ in range(count):
            values = RECORD.unpack(handle.read(RECORD.size))
            gaussian = Gaussian(values[:3], values[3:6], values[6:10], values[10:58], values[58:])
            gaussian.validate()
            yield gaussian
