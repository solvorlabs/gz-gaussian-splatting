"""SPDX-License-Identifier: Apache-2.0 — Copyright 2026 Solvor Labs.
Original synthetic scene; safe to redistribute without captured room data.
"""
from __future__ import annotations
from typing import Iterator
from .cache import Gaussian, rgb_sh


def demo_gaussians() -> Iterator[Gaussian]:
    identity = (0.0, 0.0, 0.0, 1.0)
    for row in range(31):
        for column in range(31):
            x, y = (row - 15)*0.15, (column - 15)*0.15
            rgb = (75, 105, 145) if (row//4 + column//4) % 2 else (180, 190, 210)
            yield Gaussian((x, y, 0.0), (0.1, 0.1, 0.015), identity, rgb_sh(rgb), rgb+(245,))
    for wall in range(2):
        for row in range(31):
            for column in range(18):
                p = (-2.25, (row-15)*.15, column*.15) if wall == 0 else ((row-15)*.15, 2.25, column*.15)
                scales = (.015, .1, .1) if wall == 0 else (.1, .015, .1)
                rgb = (215, 155, 85) if wall == 0 else (90, 180, 140)
                yield Gaussian(p, scales, identity, rgb_sh(rgb), rgb+(240,))
    # A red ellipsoid exposes anisotropy, occlusion and opacity falloff.
    rgb = (235, 65, 70)
    yield Gaussian((0.0, 0.0, .75), (.35, .7, .4), identity, rgb_sh(rgb), rgb+(250,))
