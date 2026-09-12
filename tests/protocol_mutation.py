#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Prove the independent oracle catches a real version-validator mutation."""
import argparse
import difflib
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile

import bootstrap_oracle as audit

ROOT = Path(__file__).resolve().parent.parent
TARGET = "src/commands.cpp"
ORIGINAL = '  if (value["protocol_version"] != "0.1")'
MUTANT = '  if (value["protocol_version"] == "__external_mutant_blocked_version__")'


class Evidence(audit.Evidence):
    def __init__(self, executable, directory):
        super().__init__(executable, directory)
        self.manifest.update(
            work_item="PR-002", example="protocol.reject_invalid",
            proof="deliberate_version_validator_mutation",
            limitations=["A temporary copy deliberately accepts unsupported versions. Passing means the independent oracle detected that defect; it does not validate the mutated executable for use."])

    def retain(self, relative, payload):
        target = self.directory / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(payload)
        self.manifest["artifacts"][relative] = {
            "path": relative, "sha256": hashlib.sha256(payload).hexdigest()}


def run(command, timeout):
    record = {"command": [str(value) for value in command], "cwd": str(ROOT),
              "started_at": audit.timestamp()}
    audit.COMMANDS.append(record)
    try:
        result = subprocess.run(command, cwd=ROOT, capture_output=True, text=True,
                                timeout=timeout, check=False)
        record.update(exit_code=result.returncode, stdout=result.stdout, stderr=result.stderr)
        return result
    except (OSError, subprocess.TimeoutExpired) as error:
        record.update(exit_code=None, error=str(error))
        raise
    finally:
        record["finished_at"] = audit.timestamp()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--evidence-dir", type=Path, required=True)
    args = parser.parse_args()
    build = args.build_dir.resolve(strict=True)
    executable_name = "omniweft_examples.exe" if sys.platform == "win32" else "omniweft_examples"
    baseline_executable = build / executable_name
    with Evidence(baseline_executable, args.evidence_dir) as evidence, tempfile.TemporaryDirectory(prefix="ow-mutant-") as temp:
        cache = dict(re.findall(r"^([A-Za-z0-9_-]+):[^=\n]+=(.*)$",
                                (build / "CMakeCache.txt").read_text(encoding="utf-8"), re.MULTILINE))
        audit.equal("mutation build uses current source", Path(cache["CMAKE_HOME_DIRECTORY"]).resolve(), ROOT)
        original_path = ROOT / TARGET
        before = original_path.read_bytes()
        text = before.decode("utf-8")
        audit.equal("version guard appears exactly once", text.count(ORIGINAL), 1)
        modified = text.replace(ORIGINAL, MUTANT)
        audit.require(modified != text, "mutation changes the real native source")
        temporary = Path(temp)
        source = temporary / "source"
        shutil.copytree(ROOT, source, ignore=shutil.ignore_patterns(
            ".git", ".cache", ".venv", "__pycache__", "build", "out", "artifacts"))
        (source / TARGET).write_text(modified, encoding="utf-8", newline="\n")
        after = (source / TARGET).read_bytes()
        evidence.retain("original-commands.cpp", before)
        evidence.retain("mutated-commands.cpp", after)
        evidence.retain("mutation.patch", "".join(difflib.unified_diff(
            text.splitlines(keepends=True), modified.splitlines(keepends=True),
            fromfile="original/"+TARGET, tofile="mutant/"+TARGET)).encode("utf-8"))
        evidence.manifest["mutation"] = {
            "source_path": TARGET, "original_source_sha256": hashlib.sha256(before).hexdigest(),
            "mutated_source_sha256": hashlib.sha256(after).hexdigest(),
            "original_guard": ORIGINAL, "mutated_guard": MUTANT,
            "expected_oracle_failure": "unknown-version: rejected"}
        mutated_build = temporary / "build"
        compiler = evidence.manifest["environment"]["compiler"]["CMAKE_CXX_COMPILER"]
        configure = run([cache["CMAKE_COMMAND"], "-S", str(source), "-B", str(mutated_build),
                         "-G", "Ninja", "-DCMAKE_BUILD_TYPE="+cache["CMAKE_BUILD_TYPE"],
                         "-DCMAKE_CXX_COMPILER="+compiler,
                         "-DCMAKE_MAKE_PROGRAM="+cache["CMAKE_MAKE_PROGRAM"],
                         "-DBUILD_TESTING=OFF"], timeout=180)
        audit.equal("mutated source configures with pinned tools", configure.returncode, 0)
        compile_result = run([cache["CMAKE_COMMAND"], "--build", str(mutated_build),
                              "--target", "omniweft_examples", "--parallel", "2"], timeout=180)
        audit.equal("mutated validator compiles", compile_result.returncode, 0)
        mutated_executable = mutated_build / executable_name
        evidence.manifest["mutation"]["executable_sha256"] = hashlib.sha256(mutated_executable.read_bytes()).hexdigest()
        oracle = run([sys.executable, str(ROOT / "tests/protocol_oracle.py"),
                      "--executable", str(mutated_executable), "--case-set", "version"], timeout=60)
        audit.equal("independent oracle rejects deliberate mutant", oracle.returncode, 1)
        audit.require("unknown-version: rejected" in oracle.stderr and
                      "schema_invalid" in oracle.stderr and "schema_valid" in oracle.stderr,
                      "oracle fails specifically because unsupported version was reported schema_valid")
        audit.equal("mutation leaves working source bytes unchanged",
                    hashlib.sha256(original_path.read_bytes()).hexdigest(), hashlib.sha256(before).hexdigest())
        evidence.retain("oracle-stdout.txt", oracle.stdout.encode("utf-8"))
        evidence.retain("oracle-stderr.txt", oracle.stderr.encode("utf-8"))
        result = {"status": "passed", "candidate_sha": evidence.manifest["candidate_sha"],
                  "mutation": evidence.manifest["mutation"], "oracle_exit_code": oracle.returncode,
                  "assertion_count": len(audit.ASSERTIONS)}
        evidence.retain_result((json.dumps(result, indent=2)+"\n").encode("utf-8"))
        print(json.dumps(result, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
