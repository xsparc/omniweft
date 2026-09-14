#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Corruption regressions for independent preregistered agent expectations."""
import copy
import hashlib
import unittest

from agents_gpu_oracle import expected_pixel, inspect_lifecycle

from agents_test_support import (FIXTURE, Failure, canonical, expected_state,
                                inspect_authoring, inspect_state, strict_json, validate_fixture)


class OracleTests(unittest.TestCase):
    def test_registered_hashes_validate(self):
        validate_fixture()

    def test_literal_states_and_bytes_validate(self):
        for revision in range(4):
            state = expected_state(revision)
            raw = canonical(state)
            inspect_authoring(state, raw.hex(), hashlib.sha256(raw).hexdigest(), revision)

    def test_corrupted_transform_is_rejected(self):
        state = expected_state(3)
        state["slots"][1]["entity"]["transform"]["position_m"][1] += 1
        with self.assertRaises(Failure):
            inspect_state(state, 3)

    def test_unchanged_geometry_wrong_authored_revision_is_rejected(self):
        state = expected_state(3)
        state["slots"][0]["entity"]["authoring_revision"] = 3
        with self.assertRaises(Failure):
            inspect_state(state, 3)

    def test_counter_float_is_rejected(self):
        state = expected_state(3)
        state["world_revision"] = 3.0
        with self.assertRaises(Failure):
            inspect_state(state, 3)

    def test_corrupted_canonical_byte_is_rejected(self):
        state = expected_state(3)
        raw = bytearray(canonical(state))
        raw[-1] ^= 1
        with self.assertRaises(Failure):
            inspect_authoring(state, raw.hex(), FIXTURE["canonical_states"][3]["sha256"], 3)

    def test_corrupted_reported_hash_is_rejected(self):
        state = expected_state(3)
        with self.assertRaises(Failure):
            inspect_authoring(state, canonical(state).hex(), "0" * 64, 3)

    def test_fixture_corruption_is_rejected_by_frozen_hash(self):
        fixture = copy.deepcopy(FIXTURE)
        fixture["objects"][0]["transform"]["position_m"][0] += 1
        with self.assertRaises(Failure):
            validate_fixture(fixture)

    def test_fixture_hash_corruption_is_rejected(self):
        fixture = copy.deepcopy(FIXTURE)
        fixture["canonical_states"][3]["sha256"] = "0" * 64
        with self.assertRaises(Failure):
            validate_fixture(fixture)

    def test_credential_or_extra_field_cannot_enter_retained_state(self):
        state = expected_state(3)
        state["token"] = "a" * 64
        with self.assertRaises(Failure):
            inspect_state(state, 3)


    def test_gpu_projection_literal_bounds(self):
        # Independently derived projected extents at 320x240:
        # left x56.6..103.4/y100..140; centre x135.2..184.8/y90..110;
        # right x218..262/y110..170. Samples use pixel centres.
        samples = ((56,120,0),(57,120,1),(102,120,1),(103,120,0),
                   (134,100,0),(135,100,2),(184,100,2),(185,100,0),
                   (160,89,0),(160,90,2),(160,109,2),(160,110,0),
                   (217,140,0),(218,140,3),(261,140,3),(262,140,0),
                   (240,169,3),(240,170,0))
        for x,y,identity in samples:
            self.assertEqual(expected_pixel(x,y,320,240,3)[0],identity)

    def lifecycle(self):
        trace = []
        for serial in (1,2):
            for event,graphics,present in (("submit",True,False),("present_queued",True,True),
                                          ("graphics_complete",False,True),("present_complete",False,False)):
                trace.append({"event":event,"submission_serial":serial,"swapchain_generation":1,
                              "graphics_pending":graphics,"present_pending":present})
        trace.append({"event":"retired","submission_serial":2,"swapchain_generation":1,
                      "graphics_pending":False,"present_pending":False})
        return {"frames":[{"submission_serial":1,"swapchain_generation":1,"frame_id":1,"simulation_tick":1,"snapshot_sequence":2},
                          {"submission_serial":2,"swapchain_generation":1,"frame_id":2,"simulation_tick":3,"snapshot_sequence":4}],
                "lifecycle_events":trace,"window_events":[],
                "runtime":{"presentation":{"enabled":True,"ready":True,"frame_count":2,"world_revision":3,"snapshot_sequence":4}}}

    def test_complete_live_gpu_trace(self):
        inspect_lifecycle(self.lifecycle())

    def test_wrong_completion_generation_is_rejected(self):
        report = self.lifecycle()
        report["lifecycle_events"][2]["swapchain_generation"] = 99
        with self.assertRaises(Failure):
            inspect_lifecycle(report)

    def test_wrong_capture_generation_is_rejected(self):
        report = self.lifecycle()
        report["frames"][1]["swapchain_generation"] = 99
        with self.assertRaises(Failure):
            inspect_lifecycle(report)

    def test_early_live_gpu_retirement_is_rejected(self):
        report = self.lifecycle()
        report["lifecycle_events"].insert(6,report["lifecycle_events"].pop())
        with self.assertRaises(Failure):
            inspect_lifecycle(report)


    def test_final_presentation_cannot_erase_captured_work(self):
        report = self.lifecycle()
        report["runtime"]["presentation"].update(frame_count=0,world_revision=0,snapshot_sequence=0)
        with self.assertRaises(Failure):
            inspect_lifecycle(report)

    def test_final_presentation_cannot_invent_submissions(self):
        report = self.lifecycle()
        report["runtime"]["presentation"]["frame_count"] = 99
        with self.assertRaises(Failure):
            inspect_lifecycle(report)

    def test_final_presentation_covers_actual_capture_publication(self):
        report = self.lifecycle()
        report["runtime"]["presentation"]["snapshot_sequence"] = 3
        with self.assertRaises(Failure):
            inspect_lifecycle(report)

    def test_wrong_attachment_source_generation_is_rejected(self):
        from pathlib import Path
        from agents_gpu_oracle import inspect_frame
        frame = dict.fromkeys(("phase","frame_id","world_id","world_revision","simulation_tick","snapshot_sequence",
                               "objects","camera","pixel_extent","source_generation","swapchain_generation",
                               "image_index","submission_serial","present_result","attachments","copy"))
        frame.update(phase="initial",frame_id=1,world_id="workshop",world_revision=1,
                     simulation_tick=1,snapshot_sequence=2,source_generation=99,
                     swapchain_generation=1,image_index=0,submission_serial=1,
                     copy={"source_generation":99})
        with self.assertRaisesRegex(Failure,"attachment source belongs"):
            inspect_frame(frame,1,"initial",Path("."),None,{"snapshot_sequence":2})

    def test_hostile_json_rejected(self):
        for raw in (b'{"value": 1, "value": 2}', b'{"value": 1e999}',
                    b'{"value": NaN}', b'{"value": "\xff"}', b'{}\x00'):
            with self.assertRaises(Failure):
                strict_json(raw)


if __name__ == "__main__":
    unittest.main()
