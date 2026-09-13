"""Regression checks for the planning gate using isolated mutated fixtures."""

import json
import shutil
import tempfile
import unittest
from pathlib import Path

from validate_plan import ROOT, validate


class PlanningGateTests(unittest.TestCase):
    def setUp(self):
        self.scratch = tempfile.TemporaryDirectory(prefix="omniweft-plan-test-")
        self.root = Path(self.scratch.name) / "repo"
        shutil.copytree(ROOT, self.root, ignore=shutil.ignore_patterns(
            ".git", "__pycache__", "artifacts", "build", "out", ".cache", ".venv"))

    def tearDown(self):
        self.scratch.cleanup()

    def alter_backlog(self, mutation):
        path = self.root / "planning/backlog.json"
        value = json.loads(path.read_text(encoding="utf-8"))
        mutation(value)
        path.write_text(json.dumps(value), encoding="utf-8")

    def test_valid_repository(self):
        self.assertEqual(validate(self.root), [])

    def test_generated_dependency_markdown_is_ignored(self):
        cached = self.root / ".cache" / "render-deps" / "upstream.md"
        cached.parent.mkdir(parents=True)
        cached.write_text("[upstream-only link](missing.md)\n", encoding="utf-8")
        self.assertEqual(validate(self.root), [])

    def test_cycle_is_rejected(self):
        self.alter_backlog(lambda data: data["items"][0]["depends_on"].append("PR-003"))
        self.assertTrue(any("Dependency cycle" in e for e in validate(self.root)))

    def test_missing_example_is_rejected(self):
        (self.root / "examples/objects-atomic.md").unlink()
        self.assertTrue(any("missing or unsafe example" in e for e in validate(self.root)))

    def test_changed_oracle_is_rejected(self):
        self.alter_backlog(lambda data: data["items"][0].update(acceptance="An undocumented oracle"))
        self.assertTrue(any("differs from backlog field acceptance" in e for e in validate(self.root)))

    def test_broken_link_and_anchor_are_rejected(self):
        with (self.root / "README.md").open("a", encoding="utf-8") as output:
            output.write("\n[missing](missing.md)\n[anchor](README.md#no-such-anchor)\n")
        errors = validate(self.root)
        self.assertTrue(any("Broken link" in e for e in errors))
        self.assertTrue(any("Missing anchor" in e for e in errors))

    def test_example_path_escape_is_rejected(self):
        self.alter_backlog(lambda data: data["items"][0].update(example_spec="../outside.md"))
        self.assertTrue(any("missing or unsafe example" in e for e in validate(self.root)))

    def test_false_completion_is_rejected(self):
        self.alter_backlog(lambda data: data.update(phase="design"))
        self.alter_backlog(lambda data: data["items"][0].update(status="done", execution={}))
        errors = validate(self.root)
        self.assertTrue(any("design phase cannot claim" in e for e in errors))
        self.assertTrue(any("done needs PR URL" in e for e in errors))

    def test_execution_without_authorization_is_rejected(self):
        self.alter_backlog(lambda data: data.update(phase="implementation"))
        self.alter_backlog(lambda data: data["items"][0].update(
            status="in_progress", execution={}))
        self.assertTrue(any("execution state needs adopted authorization" in e
                            for e in validate(self.root)))

    def test_execution_before_dependency_merge_is_rejected(self):
        self.alter_backlog(lambda data: data.update(phase="implementation"))
        self.alter_backlog(lambda data: data["items"][0].update(status="in_review"))
        self.alter_backlog(lambda data: data["items"][1].update(
            status="ready", execution={"authorization": "fixture approval"}))
        self.assertTrue(any("PR-002: dependency PR-001 is not done" in e
                            for e in validate(self.root)))

    def test_implementation_completion_without_merge_evidence_is_rejected(self):
        self.alter_backlog(lambda data: data.update(phase="implementation"))
        self.alter_backlog(lambda data: data["items"][0].update(
            status="done", execution={"authorization": "fixture approval"}))
        self.assertTrue(any("done needs PR URL, merge SHA and evidence" in e
                            for e in validate(self.root)))

    def test_dependency_and_lane_document_drift_is_rejected(self):
        path = self.root / "examples/render-world_cube.md"
        content = path.read_text(encoding="utf-8")
        content = content.replace("Dependencies: PR-003.", "Dependencies: PR-038.")
        content = content.replace("Validation lanes: cpu, gpu.", "Validation lanes: cpu.")
        path.write_text(content, encoding="utf-8")
        errors = validate(self.root)
        self.assertTrue(any("example dependency declarations differ" in e for e in errors))
        self.assertTrue(any("example lane declarations differ" in e for e in errors))

    def test_malformed_state_is_reported_without_crash(self):
        self.alter_backlog(lambda data: data["items"][0].update(status=["done"]))
        self.assertTrue(any("invalid status" in e for e in validate(self.root)))

    def test_deleted_work_item_cannot_leave_orphan_specs(self):
        self.alter_backlog(lambda data: data["items"].pop())
        errors = validate(self.root)
        self.assertTrue(any("work-item rows differ" in e for e in errors))
        self.assertTrue(any("Unregistered example specification" in e for e in errors))


if __name__ == "__main__":
    unittest.main()
