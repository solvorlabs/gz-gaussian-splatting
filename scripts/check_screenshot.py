#!/usr/bin/env python3
"""SPDX-License-Identifier: Apache-2.0 — Copyright 2026 Solvor Labs.

Dependency-free PNG smoke gate. This is a visibility regression, not parity.
"""
import struct
import sys
import zlib
from pathlib import Path


def check(path: Path) -> tuple[int, int, int]:
    data = path.read_bytes()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError("not PNG")
    offset, compressed = 8, bytearray()
    width = height = depth = colour = interlace = 0
    while offset < len(data):
        size = struct.unpack_from(">I", data, offset)[0]
        tag, payload = data[offset+4:offset+8], data[offset+8:offset+8+size]
        if len(payload) != size:
            raise ValueError("truncated PNG")
        if tag == b"IHDR":
            width, height, depth, colour, _, _, interlace = struct.unpack(">IIBBBBB", payload)
        elif tag == b"IDAT":
            compressed.extend(payload)
        offset += size + 12
    if depth != 8 or colour not in (2, 6) or interlace or not 1 <= width*height <= 40_000_000:
        raise ValueError("expected non-interlaced 8-bit RGB/RGBA PNG")
    channels = 3 if colour == 2 else 4
    row_size = width*channels
    pixels = zlib.decompress(compressed)
    if len(pixels) != height*(row_size+1):
        raise ValueError("incorrect PNG pixel length")
    previous = bytearray(row_size)
    colours, coloured = set(), 0
    red = green = blue = amber = 0
    for row_index in range(height):
        start = row_index*(row_size+1)
        kind = pixels[start]
        row = bytearray(pixels[start+1:start+1+row_size])
        if kind not in range(5):
            raise ValueError("invalid PNG filter")
        for index in range(row_size):
            left = row[index-channels] if index >= channels else 0
            up = previous[index]
            diagonal = previous[index-channels] if index >= channels else 0
            if kind == 1:
                value = left
            elif kind == 2:
                value = up
            elif kind == 3:
                value = (left+up)//2
            elif kind == 4:
                p = left+up-diagonal
                distances = (abs(p-left), abs(p-up), abs(p-diagonal))
                value = (left, up, diagonal)[distances.index(min(distances))]
            else:
                value = 0
            row[index] = (row[index]+value) & 255
        for index in range(0, row_size, channels):
            rgb = tuple(row[index:index+3])
            colours.add(rgb)
            if max(rgb)-min(rgb) > 35:
                coloured += 1
            r, g, b = rgb
            red += r > 130 and r > 1.5*g and r > 1.5*b
            green += g > 100 and g > 1.3*r and g > 1.1*b
            blue += b > 100 and b > 1.3*r and b > 1.1*g
            amber += r > 130 and g > 80 and r > 1.2*g and g > 1.3*b
        previous = row
    if len(colours) < 100 or coloured < width*height*.01:
        raise ValueError(f"blank/near-empty viewport: {len(colours)} colours; {coloured} coloured pixels")
    if min(red, green, blue, amber) < 50:
        raise ValueError(f"missing synthetic features: red={red}, green={green}, blue={blue}, amber={amber}")
    return width, height, len(colours)


if __name__ == "__main__":
    try:
        print("Visible synthetic scene:", check(Path(sys.argv[1])))
    except (OSError, ValueError, IndexError, struct.error, zlib.error) as exc:
        print(f"Screenshot smoke failed: {exc}", file=sys.stderr)
        raise SystemExit(1)
