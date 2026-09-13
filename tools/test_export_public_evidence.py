# SPDX-License-Identifier: Apache-2.0
"""Synthetic privacy and integrity tests; never uses personal fixture data."""
import hashlib
import json
from pathlib import Path
import tempfile
import unittest
from export_public_evidence import BOOTSTRAP, export

CANDIDATE = "a" * 40


class ExportTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.private = self.root / "private"
        self.private.mkdir()
        self.repo = self.root / "repo"
        self.repo.mkdir()
        self.exe = self.root / "test.exe"
        self.exe.write_bytes(b"synthetic executable")
        self.result = self.private / "result.json"
        self.result.write_bytes(json.dumps(BOOTSTRAP).encode())
        self.manifest = {
            "schema_version": 1, "candidate_sha": CANDIDATE, "candidate_worktree_dirty": False,
            "status": "passed", "exit_code": 0, "work_item": "PR-001", "example": "platform.bootstrap", "seed": 7,
            "lane": "cpu", "lanes": {"cpu": "passed", "gpu": "not_run"},
            "assertions": [{"id": 1, "assertion": "private C:\\Users\\fixture_person\\secret folder",
                            "expected": {"path": str(self.repo), "host": "fixture_machine"},
                            "actual": {"path": str(self.repo), "host": "fixture_machine"}, "status": "passed"}],
            "commands": [{"command": ["C:\\Users\\fixture_person\\python.exe", "tests/check.py"],
                          "cwd": "C:\\Users\\fixture_person", "exit_code": 0,
                          "stdout": "fixture_person fixture_machine", "stderr": ""}],
            "environment": {"os": "fixture_machine", "cpu": "private processor inventory",
                            "python": "3.12.14", "compiler": {"CMAKE_CXX_COMPILER": "C:\\Users\\fixture_person\\cl.exe",
                            "CMAKE_CXX_COMPILER_ID": "MSVC", "CMAKE_CXX_COMPILER_VERSION": "19.44"}},
            "artifacts": {
                "executable": {"path": str(self.exe), "sha256": hashlib.sha256(self.exe.read_bytes()).hexdigest()},
                "result": {"path": "result.json", "sha256": hashlib.sha256(self.result.read_bytes()).hexdigest()}}}

    def run_export(self):
        (self.private / "manifest.json").write_text(json.dumps(self.manifest), encoding="utf-8")
        return export(self.private, self.root / "public", self.repo, self.exe, CANDIDATE,
                      ("fixture_person", "fixture_machine"))

    def test_redacts_nested_private_data_and_keeps_outcomes_hashes(self):
        result = self.run_export()
        output = self.root / "public"
        text = (output / "manifest.json").read_text()
        for term in ("fixture_person", "fixture_machine", "C:\\\\Users", "private processor inventory"):
            self.assertNotIn(term, text)
        self.assertEqual(result["assertion_count"], 1)
        self.assertEqual(result["command_count"], 1)
        self.assertEqual(result["assertions"][0]["status"], "passed")
        self.assertEqual(result["commands"][0]["exit_code"], 0)
        self.assertEqual(result["lanes"]["gpu"], "not_run")
        self.assertEqual((output / "result.json").read_bytes(), self.result.read_bytes())
        self.assertFalse(result["artifacts"][0]["published"])
        self.assertFalse((output / "test.exe").exists())
        self.assertEqual((output / "manifest.sha256").read_text().strip(),
                         hashlib.sha256((output / "manifest.json").read_bytes()).hexdigest())

    def test_wrong_digest_publishes_nothing(self):
        self.manifest["artifacts"]["result"]["sha256"] = "0" * 64
        with self.assertRaisesRegex(ValueError, "hash mismatch"):
            self.run_export()
        self.assertFalse((self.root / "public").exists())

    def test_parent_escape_is_rejected(self):
        self.manifest["artifacts"]["result"]["path"] = "../outside.json"
        with self.assertRaisesRegex(ValueError, "unsafe artifact path"):
            self.run_export()

    def test_private_bytes_are_rejected(self):
        self.result.write_bytes(b'{"path":"C:\\\\Users\\\\fixture_person\\\\private.txt"}')
        self.manifest["artifacts"]["result"]["sha256"] = hashlib.sha256(self.result.read_bytes()).hexdigest()
        with self.assertRaisesRegex(ValueError, "bootstrap result schema"):
            self.run_export()
        self.assertFalse((self.root / "public").exists())

    def test_private_filename_is_rejected(self):
        target = self.private / "fixture_person.json"
        target.write_bytes(self.result.read_bytes())
        self.manifest["artifacts"]["result"]["path"] = target.name
        result = self.run_export()
        self.assertNotIn("fixture_person", json.dumps(result))

    def test_failed_assertion_cannot_export_as_passed(self):
        self.manifest["assertions"][0]["status"] = "failed"
        with self.assertRaisesRegex(ValueError, "failed assertions"):
            self.run_export()

    def test_dirty_candidate_is_rejected(self):
        self.manifest["candidate_worktree_dirty"] = True
        with self.assertRaisesRegex(ValueError, "clean candidate"):
            self.run_export()

    def test_wrong_candidate_is_rejected(self):
        self.manifest["candidate_sha"] = "b" * 40
        with self.assertRaisesRegex(ValueError, "candidate"):
            self.run_export()

    def test_symlink_is_rejected(self):
        original = self.private / "linked.json"
        try:
            original.symlink_to(self.result)
        except OSError:
            self.skipTest("symlink creation unavailable")
        self.manifest["artifacts"]["result"]["path"] = original.name
        with self.assertRaisesRegex(ValueError, "symlink"):
            self.run_export()

    def test_existing_public_directory_is_preserved(self):
        output = self.root / "public"
        output.mkdir()
        sentinel = output / "keep.txt"
        sentinel.write_text("keep")
        with self.assertRaisesRegex(ValueError, "must be new"):
            self.run_export()
        self.assertEqual(sentinel.read_text(), "keep")


    def test_untyped_seed_and_exit_codes_rejected(self):
        for key in ("seed", "exit_code", "schema_version"):
            original = self.manifest[key]
            self.manifest[key] = True
            with self.assertRaises(ValueError):
                self.run_export()
            self.manifest[key] = original

    def test_device_identifiers_never_exported(self):
        secret = {"device_uuid": "11111111-2222-3333-4444-555555555555", "serial": "ABC123", "luid": "00CAFE"}
        self.manifest["assertions"][0]["actual"] = secret
        self.manifest["assertions"][0]["expected"] = secret
        result = self.run_export()
        for value in secret.values():
            self.assertNotIn(value, json.dumps(result))
        self.assertEqual(result["assertions"][0]["actual"], {"redacted": True})

    def test_unknown_artifact_schema_only_has_commitment(self):
        self.manifest["work_item"] = "PR-003"
        self.manifest["example"] = "objects.atomic"
        self.result.write_text('{"device_uuid":"11111111-2222-3333-4444-555555555555"}')
        self.manifest["artifacts"]["result"]["sha256"] = hashlib.sha256(self.result.read_bytes()).hexdigest()
        result = self.run_export()
        self.assertFalse((self.root / "public" / "result.json").exists())
        self.assertEqual(result["artifact_count"], 2)
        self.assertEqual(len(result["artifacts"]), 2)
        self.assertNotIn("11111111", json.dumps(result))

    def test_duplicate_fields_rejected(self):
        payload = '{"schema_version":1,"schema_version":1}'
        self.result.write_text(payload)
        self.manifest["artifacts"]["result"]["sha256"] = hashlib.sha256(self.result.read_bytes()).hexdigest()
        with self.assertRaisesRegex(ValueError, "duplicate JSON"):
            self.run_export()

    def test_numeric_overflow_rejected(self):
        self.result.write_text('{"value":1e999}')
        self.manifest["artifacts"]["result"]["sha256"] = hashlib.sha256(self.result.read_bytes()).hexdigest()
        with self.assertRaisesRegex(ValueError, "nonfinite JSON"):
            self.run_export()

    def test_checked_result_is_required(self):
        del self.manifest["artifacts"]["result"]
        with self.assertRaisesRegex(ValueError, "artifact inventory"):
            self.run_export()

    def test_mutation_pass_is_explicitly_qualified(self):
        self.manifest["work_item"] = "PR-003"
        self.manifest["example"] = "objects.atomic"
        self.manifest["proof"] = "deliberate_atomic_rollback_mutation"
        result = self.run_export()
        self.assertEqual(result["evidence_kind"], "mutation_detection")
        self.assertEqual(result["proof"], "deliberate_atomic_rollback_mutation")
        self.assertIn("rejected", result["qualification"])
        self.assertEqual(result["artifacts"][0]["role"], "executable")
        self.assertEqual(result["artifacts"][1]["role"], "result")

    def test_unknown_proof_is_rejected(self):
        self.manifest["proof"] = "unrecognized"
        with self.assertRaisesRegex(ValueError, "unknown mutation"):
            self.run_export()


if __name__ == "__main__":
    unittest.main()
