# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 Solvor Labs
import math
import os
from pathlib import Path
import struct
import subprocess
import tempfile
import unittest
from xml.etree import ElementTree as ET
from gz_gaussian_splatting.cache import Gaussian, HEADER, MAGIC, RECORD, read_cache, rgb_sh, write_cache
from gz_gaussian_splatting.ply import load_ply
from gz_gaussian_splatting.scene import write_world
from gz_gaussian_splatting.cli import main


class Contracts(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)
        self.gaussian = Gaussian((1., 2., 3.), (.1, .2, .3), (0., 0., 0., 1.),
                                 rgb_sh((200, 40, 80)), (200, 40, 80, 220))

    def tearDown(self):
        self.temporary.cleanup()

    def ply(self, *, binary=False, degree=3, invalid=False):
        names = ["x", "y", "z", "opacity", "scale_0", "scale_1", "scale_2",
                 "rot_0", "rot_1", "rot_2", "rot_3", "f_dc_0", "f_dc_1", "f_dc_2"]
        rest_count = 3*((degree+1)**2-1)
        names += [f"f_rest_{i}" for i in range(rest_count)]
        values = [1., 2., 3., 0., math.log(.1), math.log(.2), math.log(.3),
                  1., 0., 0., 0., .2, .3, .4] + [i*.01 for i in range(rest_count)]
        fmt = "binary_little_endian" if binary else "ascii"
        header = "ply\nformat "+fmt+" 1.0\nelement vertex 1\n"
        header += "".join(f"property float {name}\n" for name in names)+"end_header\n"
        body = struct.pack("<"+"f"*len(values), *values) if binary else (" ".join(map(str, values))+"\n").encode()
        path = self.root / "input.ply"
        path.write_bytes(header.encode() + (body[:-5] if invalid else body))
        return path

    def test_cache_roundtrip_and_abi(self):
        path = self.root / "scene.gzgs"
        write_cache(path, [self.gaussian])
        self.assertEqual(path.stat().st_size, 12+236)
        restored = list(read_cache(path))[0]
        self.assertEqual(restored.position, self.gaussian.position)
        self.assertEqual(restored.rgba, self.gaussian.rgba)

    def test_atomic_failure_leaves_no_output(self):
        path = self.root / "bad.gzgs"
        bad = Gaussian((math.nan, 0., 0.), (.1, .1, .1), (0., 0., 0., 1.), rgb_sh((0, 0, 0)), (0, 0, 0, 255))
        with self.assertRaises(ValueError):
            write_cache(path, [self.gaussian, bad])
        self.assertFalse(path.exists())

    def test_cache_rejects_truncated_and_trailing(self):
        path = self.root / "bad.gzgs"
        path.write_bytes(HEADER.pack(MAGIC, 1))
        with self.assertRaises(ValueError):
            list(read_cache(path))
        write_cache(path, [self.gaussian], force=True)
        with path.open("ab") as handle:
            handle.write(b"x")
        with self.assertRaises(ValueError):
            list(read_cache(path))

    def test_output_does_not_clobber(self):
        path = self.root / "scene.gzgs"
        write_cache(path, [self.gaussian])
        with self.assertRaises(FileExistsError):
            write_cache(path, [self.gaussian])

    def test_standard_ply_ascii_binary(self):
        for binary in (False, True):
            point = list(load_ply(self.ply(binary=binary)))[0]
            self.assertAlmostEqual(point.scale[0], .1, places=6)
            self.assertEqual(point.quaternion_xyzw, (0., 0., 0., 1.))
            self.assertEqual(point.rgba[3], 128)
            self.assertAlmostEqual(point.sh[3], 0.)
            self.assertAlmostEqual(point.sh[4], .15, places=6)
            self.assertAlmostEqual(point.sh[5], .30, places=6)

    def test_all_sh_degrees_preserved(self):
        for degree in range(4):
            point = list(load_ply(self.ply(degree=degree)))[0]
            self.assertEqual(len(point.sh), 48)
            start = 3*(degree+1)**2
            self.assertTrue(all(v == 0 for v in point.sh[start:]))

    def test_truncated_binary_rejected(self):
        with self.assertRaises(ValueError):
            list(load_ply(self.ply(binary=True, invalid=True)))

    def test_translation_and_uniform_scale(self):
        point = list(load_ply(self.ply(), scale=2, translate=(10, 0, 0)))[0]
        self.assertEqual(point.position, (12, 4, 6))
        self.assertAlmostEqual(point.scale[2], .6)

    def test_captured_world_validation(self):
        cache = self.root / "room.gzgs"
        write_cache(cache, [self.gaussian])
        world = self.root / "room.sdf"
        with self.assertRaises(ValueError):
            write_world(cache, world, mode="captured")
        write_world(cache, world, mode="captured", camera_pose=(0, 0, 0, 0, 0, 0),
                    intrinsics=(640, 480, 500, 500, 320, 240))
        plugin = ET.parse(world).find(".//plugin[@filename='GaussianSplatRenderer']")
        self.assertEqual(plugin.findtext("cache_path"), str(cache.resolve()))
        self.assertEqual(plugin.findtext("capture_width"), "640")

    def test_rgb_cloud_requires_explicit_radius(self):
        path = self.root / "rgb.ply"
        path.write_text("ply\nformat ascii 1.0\nelement vertex 1\nproperty float x\nproperty float y\nproperty float z\nproperty uchar red\nproperty uchar green\nproperty uchar blue\nend_header\n0 0 0 255 0 0\n")
        with self.assertRaises(ValueError):
            list(load_ply(path))
        self.assertEqual(list(load_ply(path, point_radius=.1))[0].rgba, (255, 0, 0, 255))

    def test_big_endian_ply(self):
        path = self.ply(binary=True)
        header, data = path.read_bytes().split(b"end_header\n", 1)
        values = struct.unpack("<"+"f"*(len(data)//4), data)
        path.write_bytes(header.replace(b"binary_little_endian", b"binary_big_endian")+
                         b"end_header\n"+struct.pack(">"+"f"*len(values), *values))
        self.assertEqual(list(load_ply(path))[0].position, (1, 2, 3))

    def test_nonfinite_ply_and_incomplete_schema_rejected(self):
        path = self.ply()
        path.write_bytes(path.read_bytes().replace(b"1.0 2.0", b"nan 2.0"))
        with self.assertRaises(ValueError):
            list(load_ply(path))
        path = self.ply()
        path.write_bytes(path.read_bytes().replace(b"scale_1", b"bad_scale"))
        with self.assertRaises(ValueError):
            list(load_ply(path, point_radius=.1))

    def test_opacity_filter_and_empty_atomic_output(self):
        path = self.root / "empty.gzgs"
        with self.assertRaises(ValueError):
            write_cache(path, load_ply(self.ply(), min_opacity=.9))
        self.assertFalse(path.exists())

    def test_invalid_world_capture_and_overwrite(self):
        cache, world = self.root/"scene.gzgs", self.root/"scene.sdf"
        write_cache(cache, [self.gaussian])
        with self.assertRaises(ValueError):
            write_world(cache, world, screenshot=cache)
        with self.assertRaises(ValueError):
            write_world(cache, world, pose_topic="/pose")
        write_world(cache, world)
        with self.assertRaises(FileExistsError):
            write_world(cache, world)

    def test_tiny_scale_not_serialized_as_zero(self):
        bad = Gaussian((0., 0., 0.), (1e-100, .1, .1), (0., 0., 0., 1.),
                       rgb_sh((0, 0, 0)), (0, 0, 0, 255))
        with self.assertRaises(ValueError):
            bad.validate()

    def test_cli_generated_demo(self):
        self.assertEqual(main(["demo", "--output-dir", str(self.root/"demo")]), 0)
        self.assertEqual(len(list(read_cache(self.root/"demo/room.gzgs"))), 2078)

    @unittest.skipUnless(os.environ.get("GZGS_CACHE_PROBE"), "native probe provided by CTest")
    def test_python_writer_cpp_reader(self):
        path = self.root/"interop.gzgs"
        write_cache(path, [self.gaussian])
        fields = subprocess.check_output([os.environ["GZGS_CACHE_PROBE"], str(path)], text=True).split()
        self.assertEqual(fields, ["1", "1", "2", "3", "0.2", "1", "220"])
        data = bytearray(path.read_bytes())
        data[12:16] = struct.pack("<f", math.nan)
        path.write_bytes(data)
        result = subprocess.run([os.environ["GZGS_CACHE_PROBE"], str(path)], capture_output=True)
        self.assertEqual(result.returncode, 1)


if __name__ == "__main__":
    unittest.main()
