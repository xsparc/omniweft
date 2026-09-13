#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""CPU tests of the independent oracle's rejection and privacy behavior; no GPU claim."""
import copy
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest
from unittest.mock import patch

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

    def lifecycle_fixture(self):
        frames = [{"frame_id":i, "submission_serial":i, "swapchain_generation":1 if i <= 2 else 2,
                   "pixel_extent":{"width":320 if i <= 2 else 400,"height":240 if i <= 2 else 300}}
                  for i in range(1,5)]
        events = [{"kind":kind,"pixel_extent":{"width":400,"height":300},"minimized":kind == "minimized",
                   "submission_serial":2 if kind == "resized" else 3,"swapchain_generation":1 if kind == "resized" else 2}
                  for kind in ("resized","minimized","restored")]
        trace = []
        def add(event,serial,generation,graphics,present):
            trace.append({"event":event,"submission_serial":serial,"swapchain_generation":generation,
                          "graphics_pending":graphics,"present_pending":present})
        for serial in range(1,5):
            generation = 1 if serial <= 2 else 2
            add("submit",serial,generation,True,False)
            add("present_queued",serial,generation,True,True)
            if serial == 2:
                add("resize_requested",serial,generation,True,True)
            add("graphics_complete",serial,generation,False,True)
            add("present_complete",serial,generation,False,False)
            if serial in (2,4):
                add("retired",serial,generation,False,False)
        return {"frames":frames,"window_events":events,"lifecycle_events":trace}

    def test_lifecycle_requires_actual_completion_before_retirement(self):
        report = self.lifecycle_fixture()
        oracle.inspect_lifecycle(report)
        mutated = copy.deepcopy(report)
        trace = mutated["lifecycle_events"]
        retired = next(i for i,e in enumerate(trace) if e["event"] == "retired")
        trace[retired-1],trace[retired] = trace[retired],trace[retired-1]
        with self.assertRaisesRegex(Failure,"retirement follows graphics and present"):
            oracle.inspect_lifecycle(mutated)

    def test_lifecycle_rejects_wrong_generation_completion(self):
        report = self.lifecycle_fixture()
        next(e for e in report["lifecycle_events"] if e["event"] == "graphics_complete")["swapchain_generation"] = 99
        with self.assertRaisesRegex(Failure,"graphics completion"):
            oracle.inspect_lifecycle(report)

    def test_unused_swapchain_generation_can_retire_safely(self):
        report = self.lifecycle_fixture()
        report["lifecycle_events"].append({"event":"retired","submission_serial":4,"swapchain_generation":3,
                                           "graphics_pending":False,"present_pending":False})
        oracle.inspect_lifecycle(report)

    def test_lifecycle_rejects_wrong_captured_generation(self):
        report = self.lifecycle_fixture()
        report["frames"][0]["swapchain_generation"] = 99
        with self.assertRaisesRegex(Failure,"captured frame generation"):
            oracle.inspect_lifecycle(report)

    def test_other_checkout_cannot_receive_current_candidate_label(self):
        with tempfile.TemporaryDirectory(prefix="ow-provenance-test-") as temporary:
            build = Path(temporary)/"build"; build.mkdir()
            executable = build/"omniweft_examples"; executable.write_bytes(b"synthetic executable")
            (build/"CMakeCache.txt").write_text("CMAKE_HOME_DIRECTORY:INTERNAL="+str(Path(temporary)/"other-source")+"\\n",encoding="utf-8")
            output = Path(temporary)/"evidence"
            with self.assertRaisesRegex(Failure,"CMake source matches"):
                Evidence(output,executable,"headless")
            self.assertFalse(output.exists())

    def test_failed_assertion_cannot_be_finalized_as_passed(self):
        with tempfile.TemporaryDirectory(prefix="ow-failed-evidence-") as temporary:
            output = Path(temporary)/"evidence"
            with patch.object(Evidence,"_build_provenance",return_value={"unit_fixture":True}):
                evidence = Evidence(output,Path(sys.executable),"headless")
            with self.assertRaises(Failure):
                oracle.require(False,"synthetic assertion failure")
            with self.assertRaisesRegex(Failure,"failed assertion"):
                evidence.finish("passed")
            self.assertEqual(json.loads((output/"manifest.json").read_text(encoding="utf-8"))["status"],"failed")

    def test_duplicate_native_json_cannot_erase_a_validation_error(self):
        with tempfile.TemporaryDirectory(prefix="ow-json-test-") as temporary:
            output = Path(temporary)
            (output/"result.json").write_text('{"validation":{"errors":7,"errors":0}}',encoding="utf-8")
            with self.assertRaisesRegex(Failure,"duplicate JSON key"):
                oracle.read_report(output)
            (output/"result.json").write_text('{"depth":NaN}',encoding="utf-8")
            with self.assertRaisesRegex(Failure,"nonfinite JSON"):
                oracle.read_report(output)
            (output/"result.json").write_text('{"depth":1e309}',encoding="utf-8")
            with self.assertRaisesRegex(Failure,"nonfinite JSON"):
                oracle.read_report(output)

    def test_raw_process_text_never_enters_manifest(self):
        with tempfile.TemporaryDirectory(prefix="ow-privacy-test-") as temporary:
            directory = Path(temporary)/"evidence"
            with patch.object(Evidence,"_build_provenance",return_value={"unit_fixture":True}):
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
            with patch.object(Evidence,"_build_provenance",return_value={"unit_fixture":True}):
                evidence = Evidence(Path(temporary)/"evidence",Path(sys.executable),"headless")
            evidence.retain("frame.bin",b"first")
            with self.assertRaisesRegex(Failure,"never overwrites"):
                evidence.retain("frame.bin",b"second")


if __name__ == "__main__":
    unittest.main()

