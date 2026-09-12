#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Independent black-box bootstrap oracle; never imports native fixture logic."""
import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parent.parent
ASSERTIONS = []
COMMANDS = []

EXPECTED = {
    "schema_version": 1,
    "example": "platform.bootstrap",
    "mode": "headless",
    "seed": 7,
    "lifecycle": ["created", "running", "stopped"],
    "steps": 1,
    "fixture_checksum": 1282168116,
    "verified": True,
}


def require(condition, message):
    ASSERTIONS.append({"id": len(ASSERTIONS) + 1, "assertion": message,
                       "expected": True, "actual": bool(condition),
                       "status": "passed" if condition else "failed"})
    if not condition:
        raise AssertionError(message)


def equal(name, actual, expected):
    passed = actual == expected
    ASSERTIONS.append({"id": len(ASSERTIONS) + 1, "assertion": name,
                       "expected": expected, "actual": actual,
                       "status": "passed" if passed else "failed"})
    if not passed:
        raise AssertionError(f"{name}: expected {expected!r}, got {actual!r}")


def timestamp():
    return datetime.now(timezone.utc).isoformat()


def execute(command, cwd=None):
    record = {"command": [str(arg) for arg in command], "cwd": str(cwd or Path.cwd()),
              "started_at": timestamp()}
    COMMANDS.append(record)
    try:
        result = subprocess.run(command, cwd=cwd, capture_output=True, text=True, timeout=5, check=False)
        record.update(exit_code=result.returncode, stdout=result.stdout, stderr=result.stderr)
        return result
    except (OSError, subprocess.TimeoutExpired) as error:
        record.update(exit_code=None, error=str(error))
        raise
    finally:
        record["finished_at"] = timestamp()


class Evidence:
    """Retain results and observed build metadata only when explicitly requested."""
    def __init__(self, executable, directory):
        self.executable = executable
        self.directory = directory.resolve() if directory else None
        self.manifest = {"schema_version": 1, "work_item": "PR-001", "example": "platform.bootstrap",
                         "seed": 7, "lane": "cpu", "started_at": timestamp(),
                         "timestamp_utc": timestamp(), "command": [sys.executable, *sys.argv],
                         "lanes": {"cpu": "not_run", "gpu": "not_applicable"},
                         "assertions": ASSERTIONS, "commands": COMMANDS, "artifacts": {},
                         "limitations": ["Headless lifecycle fixture only; no graphics, physics, or world engine validation."]}

    def __enter__(self):
        if self.directory:
            self.directory.mkdir(parents=True, exist_ok=False)
            try:
                self.inspect_build()
            except Exception as error:
                self.finish(error)
                raise
        return self

    def inspect_build(self):
        head = execute(["git", "rev-parse", "HEAD"], ROOT)
        require(head.returncode == 0 and re.fullmatch(r"[0-9a-f]{40}\n?", head.stdout) is not None,
                "candidate SHA must resolve to a full Git commit")
        dirty = execute(["git", "status", "--porcelain"], ROOT)
        require(dirty.returncode == 0, "working-tree status must be observable")
        self.manifest.update(candidate_sha=head.stdout.strip(), candidate_worktree_dirty=bool(dirty.stdout.strip()))
        cache_path = self.executable.parent / "CMakeCache.txt"
        cache = dict(re.findall(r"^([A-Za-z0-9_-]+):[^=\n]+=(.*)$", cache_path.read_text(encoding="utf-8"), re.MULTILINE))
        cmake_version = ".".join(cache[f"CMAKE_CACHE_{part}_VERSION"] for part in ("MAJOR", "MINOR", "PATCH"))
        compiler_file = self.executable.parent / "CMakeFiles" / cmake_version / "CMakeCXXCompiler.cmake"
        compiler = dict(re.findall(r'set\((CMAKE_CXX_COMPILER(?:_ID|_VERSION)?) "([^"]+)"\)',
                                   compiler_file.read_text(encoding="utf-8")))
        for field in ("CMAKE_CXX_COMPILER", "CMAKE_CXX_COMPILER_ID", "CMAKE_CXX_COMPILER_VERSION"):
            require(bool(compiler.get(field)), f"observed build metadata must include {field}")
        equal("build source matches tested repository", Path(cache["CMAKE_HOME_DIRECTORY"]).resolve(), ROOT)
        ninja = execute([cache["CMAKE_MAKE_PROGRAM"], "--version"])
        require(ninja.returncode == 0 and bool(ninja.stdout.strip()), "actual Ninja version must be observable")
        self.manifest["environment"] = {
            "os": platform.platform(), "machine": platform.machine(), "cpu": platform.processor(),
            "logical_cpu_count": os.cpu_count(), "python": platform.python_version(),
            "compiler": compiler, "cmake": cmake_version, "ninja": ninja.stdout.strip(),
            "configuration": cache["CMAKE_BUILD_TYPE"], "generator": cache["CMAKE_GENERATOR"],
        }
        self.manifest["artifacts"]["executable"] = {
            "path": str(self.executable), "sha256": hashlib.sha256(self.executable.read_bytes()).hexdigest()}
        self.manifest["github_run"] = {key: os.environ[key] for key in ("GITHUB_SHA", "GITHUB_RUN_ID", "GITHUB_RUN_ATTEMPT") if key in os.environ}

    def retain_result(self, payload):
        if self.directory:
            (self.directory / "result.json").write_bytes(payload)
            self.manifest["artifacts"]["result"] = {"path": "result.json", "sha256": hashlib.sha256(payload).hexdigest()}

    def finish(self, error):
        if self.directory:
            outcome = "failed" if error else "passed"
            self.manifest.update(finished_at=timestamp(), status=outcome, exit_code=1 if error else 0)
            self.manifest["lanes"]["cpu"] = outcome
            if error:
                self.manifest["error"] = str(error)
            (self.directory / "manifest.json").write_text(json.dumps(self.manifest, indent=2, default=str) + "\n", encoding="utf-8")

    def __exit__(self, kind, error, traceback):
        if error is None and self.directory:
            try:
                require("result" in self.manifest["artifacts"], "successful evidence must retain the checked native result")
            except Exception as validation_error:
                self.finish(validation_error)
                raise
        self.finish(error)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", required=True, type=Path)
    parser.add_argument("--evidence-dir", type=Path, help="Retain manifest and checked result in a fresh directory")
    args = parser.parse_args()
    executable = args.executable.resolve(strict=True)
    base = ["--example", "platform.bootstrap", "--headless", "--seed", "7", "--verify"]
    checks = []

    def run(argv, code, diagnostic=None):
        result = execute([str(executable), *argv])
        equal(f"command {len(COMMANDS)} exit code", result.returncode, code)
        if diagnostic:
            require(diagnostic in result.stderr, f"{argv}: missing actionable diagnostic {diagnostic}: {result.stderr}")
        return result

    with Evidence(executable, args.evidence_dir) as evidence, tempfile.TemporaryDirectory(prefix="omniweft-bootstrap-") as temp:
        root = Path(temp)
        first = root / "success with spaces"
        run([*base, "--output", str(first)], 0)
        payload = (first / "result.json").read_bytes()
        actual = json.loads(payload)
        equal("native result fields", sorted(actual), sorted(EXPECTED))
        for field, expected in EXPECTED.items():
            equal(f"native result {field}", actual.get(field), expected)
        evidence.retain_result(payload)
        require(sorted(p.name for p in first.iterdir()) == ["result.json"], "leftover staging file")
        checks.append("seed7_lifecycle_literal_oracle")
        second = root / "repeat"
        run([*base, "--output", str(second)], 0)
        require((second / "result.json").read_bytes() == payload, "same fixture produced different bytes")
        checks.append("repeat_byte_identity")
        run([*base, "--output", str(first)], 5, "choose a new --output directory")
        require((first / "result.json").read_bytes() == payload, "existing result was overwritten")
        checks.append("existing_artifact_preserved")
        existing = root / "existing-empty"
        existing.mkdir()
        run([*base, "--output", str(existing)], 5, "choose a new --output directory")
        require(not list(existing.iterdir()), "existing empty directory was modified")
        checks.append("existing_empty_directory_preserved")

        malformed = [
            [], ["--help", "--verify"], [*base, "--unknown"], [*base, "--headless"],
            [*base, "--seed", "7"], [*base, "--output"],
            ["--example", "unknown", "--headless", "--seed", "7"],
        ]
        for seed in ("", "-1", "+7", "7x", "4294967296", "8"):
            malformed.append(["--example", "platform.bootstrap", "--headless", "--seed", seed, "--verify"])
        for index, case in enumerate(malformed):
            output = root / f"invalid-{index}"
            run([*case, "--output", str(output)], 2, "invalid_arguments:")
            require(not output.exists(), f"invalid CLI created output: {case}")
        checks.append(f"malformed_cli_{len(malformed)}_cases")

        graphics = root / "graphics"
        run([arg for arg in base if arg != "--headless"] + ["--output", str(graphics)], 3, "rerun with --headless")
        require(not graphics.exists(), "unavailable graphics published output")
        checks.append("graphics_unavailable_without_device_probe")

        blocked = root / "blocked"
        blocked.write_text("preserve caller data", encoding="utf-8")
        run([*base, "--output", str(blocked / "recovered")], 5, "writable, new --output directory")
        require(blocked.read_text(encoding="utf-8") == "preserve caller data", "output failure changed caller data")
        blocked.unlink()
        recovered = blocked / "recovered"
        run([*base, "--output", str(recovered)], 0)
        require(json.loads((recovered / "result.json").read_bytes()) == EXPECTED, "recovery did not complete")
        checks.append("failed_output_then_recovery")
        run(["--help"], 0)
        checks.append("help")
        print(json.dumps({"status": "passed", "checks": checks,
                          "artifact_sha256": hashlib.sha256(payload).hexdigest()}, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
