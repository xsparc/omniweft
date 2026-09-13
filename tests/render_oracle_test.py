#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""CPU tests of the independent oracle's rejection and privacy behavior; no GPU claim."""
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest

import render_oracle as oracle
from render_test_support import Evidence, Failure


class ReadbackOracleTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.width,cls.height = 80,60
        cls.labels,cls.depths,cls.band = oracle.expected_image(cls.width,cls.height,1)
        cls.color = bytes(channel for label in cls.labels for channel in
                          (oracle.COLORS[label-1] if label else oracle.CLEAR))
        cls.ids = b"".join(struct.pack("<I",int(label != 0)) for label in cls.labels)
        cls.depth = b"".join(struct.pack("<f",depth) for depth in cls.depths)
        cls.interior = next(i for i,label in enumerate(cls.labels) if label and not cls.band[i])
        cls.background = next(i for i,label in enumerate(cls.labels) if not label and not cls.band[i])
        cls.edge = next(i for i,value in enumerate(cls.band) if value)

    def check_image(self,color=None,ids=None,depth=None,fmt="R8G8B8A8_UNORM"):
        return oracle.validate_pixels(self.color if color is None else color,self.ids if ids is None else ids,
                                      self.depth if depth is None else depth,self.width,self.height,1,fmt,"synthetic")

    def test_literal_ray_arithmetic(self):
        oracle.oracle_math_selfcheck()

    def test_consistent_fixture_and_bgra(self):
        result = self.check_image()
        self.assertGreater(result["tested_pixels"],0)
        bgra = bytearray(self.color)
        for i in range(self.width*self.height):
            bgra[i*4],bgra[i*4+2] = bgra[i*4+2],bgra[i*4]
        self.check_image(color=bytes(bgra),fmt="B8G8R8A8_UNORM")

    def test_blank_cube_cannot_pass(self):
        with self.assertRaisesRegex(Failure,"GPU ID mask"):
            self.check_image(ids=bytes(len(self.ids)))

    def test_untouched_background_is_required(self):
        color = bytearray(self.color); color[self.background*4] += 2
        with self.assertRaisesRegex(Failure,"background color"):
            self.check_image(color=bytes(color))

    def test_depth_wrong_interior_rejects(self):
        depth = bytearray(self.depth); struct.pack_into("<f",depth,self.interior*4,.9)
        with self.assertRaisesRegex(Failure,"normalized depth"):
            self.check_image(depth=bytes(depth))

    def test_edge_band_does_not_hide_unknown_ids(self):
        ids = bytearray(self.ids); struct.pack_into("<I",ids,self.edge*4,99)
        with self.assertRaisesRegex(Failure,"known everywhere"):
            self.check_image(ids=bytes(ids))

    def test_edge_band_does_not_hide_nonfinite_depth(self):
        depth = bytearray(self.depth); struct.pack_into("<f",depth,self.edge*4,float("nan"))
        with self.assertRaisesRegex(Failure,"finite and normalized everywhere"):
            self.check_image(depth=bytes(depth))

    def test_truncated_attachment_rejects(self):
        with self.assertRaisesRegex(Failure,"byte lengths"):
            self.check_image(color=self.color[:-1])

    def test_int_identity_tokens_remain_strict(self):
        with self.assertRaisesRegex(Failure,"integer"):
            oracle.exact({"generation":1.0},{"generation":1},"identity")

    def test_raw_process_text_never_enters_manifest(self):
        with tempfile.TemporaryDirectory(prefix="ow-privacy-test-") as temporary:
            directory = Path(temporary)/"evidence"
            evidence = Evidence(directory,Path(sys.executable),"headless")
            result = evidence.run([sys.executable,"-c","print('PRIVATE_ID_CANARY /home/synthetic-person')"],
                                  ["<python>","<synthetic-output-check>"])
            self.assertIn("PRIVATE_ID_CANARY",result.stdout)
            evidence.finish("passed")
            content = (directory/"manifest.json").read_text(encoding="utf-8")
            self.assertNotIn("PRIVATE_ID_CANARY",content)
            self.assertNotIn("synthetic-person",content)
            self.assertNotIn(str(Path(sys.executable)),content)
            self.assertNotIn("hostname",json.loads(content)["environment"])

    def test_evidence_cannot_overwrite_an_earlier_run(self):
        with tempfile.TemporaryDirectory(prefix="ow-evidence-test-") as temporary:
            evidence = Evidence(Path(temporary)/"evidence",Path(sys.executable),"headless")
            evidence.retain("frame.bin",b"first")
            with self.assertRaisesRegex(Failure,"never overwrites"):
                evidence.retain("frame.bin",b"second")


if __name__ == "__main__":
    unittest.main()
