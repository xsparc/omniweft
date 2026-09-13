#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Focused regressions for the independent SDK oracle and privacy boundary."""
import copy
from pathlib import Path
import tempfile
import unittest
from unittest import mock

import sdk_oracle as oracle
from sdk_client_probe import reply
from sdk_test_support import Evidence, Failure, canonical, parse_response, request_bytes, strict_json


class OracleTests(unittest.TestCase):
    def test_literal_world_binary_layout(self):
        self.assertEqual(len(canonical(oracle.snapshot(0))), 40)
        value = canonical(oracle.snapshot(2))
        self.assertEqual(len(value), 195)
        self.assertEqual(value[:20], b"OWOBJ001\x08\x00\x00\x00workshop")
        self.assertEqual(value[28:36], b"\x02\x00\x00\x00\x00\x00\x00\x00")
        self.assertEqual(value[115:139].hex(), "0000000000000440000000000000f0bf0000000000000840")

    def test_full_state_detects_allocator_and_authored_changes(self):
        original = oracle.snapshot(2)
        for field, value in (("generation", 2), ("retired", True)):
            changed = copy.deepcopy(original)
            changed["slots"][0][field] = value
            self.assertNotEqual(canonical(original), canonical(changed))
        changed = copy.deepcopy(original)
        changed["slots"][0]["entity"]["authoring_revision"] = 9
        self.assertNotEqual(canonical(original), canonical(changed))
        self.assertNotEqual(canonical(original), canonical(oracle.snapshot(3)))

    def test_canonical_negative_zero_normalizes(self):
        original = oracle.snapshot(1)
        changed = copy.deepcopy(original)
        changed["slots"][0]["entity"]["transform"]["position_m"][0] = -0.0
        self.assertEqual(canonical(original), canonical(changed))

    def test_json_rejects_duplicates_even_decoded_keys(self):
        for body in (b'{"key":1,"key":2}', b'{"key":1,"k\\u0065y":2}'):
            with self.subTest(body=body), self.assertRaises(Failure):
                strict_json(body)

    def test_json_rejects_nonfinite_and_invalid_utf8(self):
        for body in (b'{"x":NaN}', b'{"x":Infinity}', b'{"x":1e999}', b'{"x":"\xff"}'):
            with self.subTest(body=body), self.assertRaises(Failure):
                strict_json(body)

    def test_response_parser_requires_actual_exact_length(self):
        self.assertEqual(parse_response(reply({"ok": True})), (200, {"ok": True}))
        for body in (reply(body=b"{}", length=3), reply(body=b"{}extra", length=2),
                     reply(body=b"{}", extra=[("Content-Length", "2")])):
            with self.assertRaises(Failure):
                parse_response(body)

    def test_response_parser_rejects_browser_grant(self):
        with self.assertRaisesRegex(Failure, "browser"):
            parse_response(reply({}, extra=[("Access-Control-Allow-Origin", "*")]))

    def test_secret_rejected_before_artifact_creation(self):
        with tempfile.TemporaryDirectory() as directory:
            evidence = Evidence.__new__(Evidence)
            evidence.secrets = {"a" * 64}
            evidence.directory = Path(directory)
            evidence.retention_prefix = ""
            evidence.manifest = {"artifacts": {}}
            with self.assertRaisesRegex(Failure, "credentials"):
                evidence.retain("result.json", ('{"secret":"' + "a" * 64 + '"}').encode())
            self.assertEqual(list(Path(directory).iterdir()), [])

    def test_auth_mutation_reports_state_before_http_status(self):
        descriptor = {"host": "127.0.0.1", "port": 10000, "token": "a" * 64, "epoch": "b" * 64}
        host = type("HostFixture", (), {"descriptor": descriptor})()
        scenario = oracle.Scenario(host, None, "fixture")
        scenario.expected = oracle.snapshot(2)
        scenario.sequence = 3
        observation = {"protocol_version": "0.1", "epoch": descriptor["epoch"],
                       "next_sequence": 4, "snapshot": oracle.snapshot(3)}
        with mock.patch("sdk_oracle.exchange", side_effect=[(200, {}), (200, observation)]):
            with self.assertRaisesRegex(Failure, "wrong-token: complete world unchanged"):
                scenario.denied("wrong-token", b"unused", 401, "NOT_AUTHORIZED")

    def test_request_builder_does_not_reuse_ambient_credentials(self):
        descriptor = {"host": "127.0.0.1", "port": 10000, "token": "a" * 64, "epoch": "b" * 64}
        request = request_bytes(descriptor, "GET", "/v0/capabilities", change={"authorization": None})
        self.assertNotIn(b"Authorization:", request)
        self.assertNotIn(b"Cookie:", request)
        self.assertIn(b"Host: 127.0.0.1:10000\r\n", request)


if __name__ == "__main__":
    unittest.main()
