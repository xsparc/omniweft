# SPDX-License-Identifier: Apache-2.0
"""Small private-by-default evidence recorder for the presentation tests."""
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parent.parent
CHECKS = []


class Failure(Exception):
    """Only fixed test-authored labels may be emitted publicly."""


def require(condition, label):
    CHECKS.append({"assertion": label, "passed": bool(condition)})
    if not condition:
        raise Failure(label)


def exact(actual, expected, label):
    # Do not dump arbitrary native values, paths or vendor strings on failure.
    if type(expected) is dict:
        require(type(actual) is dict and actual.keys() == expected.keys(), label + ": fields")
        for key in expected:
            exact(actual[key], expected[key], label + "/" + key)
    elif type(expected) is list:
        require(type(actual) is list and len(actual) == len(expected), label + ": length")
        for i, value in enumerate(expected):
            exact(actual[i], value, label + "/" + str(i))
    elif type(expected) is int:
        require(type(actual) is int and actual == expected, label + ": integer")
    elif type(expected) is float:
        require(type(actual) in (float, int) and actual == expected, label + ": number")
    else:
        require(type(actual) is type(expected) and actual == expected, label)


def timestamp():
    return datetime.now(timezone.utc).isoformat()


def digest(payload):
    return hashlib.sha256(payload).hexdigest()


def safe_relative(value):
    require(type(value) is str and re.fullmatch(r"[A-Za-z0-9_.-]+(?:/[A-Za-z0-9_.-]+)*", value) is not None,
            "artifact path is repository-relative")
    require(all(part not in (".", "..") for part in value.split("/")), "artifact path stays within output")
    return value


class Evidence:
    def __init__(self, directory, executable, mode, proof="independent_oracle"):
        self.directory = Path(directory) if directory else None
        self.retention_prefix = ""
        candidate = subprocess.run(["git", "rev-parse", "HEAD"], cwd=ROOT,
                                   capture_output=True, text=True, check=False)
        sha = candidate.stdout.strip()
        require(candidate.returncode == 0 and re.fullmatch("[0-9a-f]{40}", sha) is not None,
                "candidate is an exact commit")
        dirty = subprocess.run(["git", "status", "--porcelain"], cwd=ROOT,
                               capture_output=True, text=True, check=False)
        require(dirty.returncode == 0, "candidate dirty status is known")
        self.manifest = {
            "schema_version": 1, "work_item": "PR-004", "example": "render.world_cube",
            "candidate_sha": sha, "dirty": bool(dirty.stdout), "mode": mode, "proof": proof,
            "timestamp_utc": timestamp(), "status": "running",
            "environment": {"os": "Windows" if sys.platform == "win32" else "Linux" if sys.platform.startswith("linux") else "other",
                            "python_version": sys.version.split()[0]},
            "executable_sha256": digest(Path(executable).read_bytes()),
            "commands": [], "assertions": [], "artifacts": {},
            "privacy": "Allowlisted metadata; raw process output, environment, host/device identifiers and absolute paths are never persisted.",
        }
        if self.directory:
            require(not self.directory.exists(), "evidence output is fresh")
            self.directory.mkdir(parents=True)

    def retain(self, relative, payload):
        relative = safe_relative((self.retention_prefix + "/" if self.retention_prefix else "") + relative)
        require(relative not in self.manifest["artifacts"], "retained evidence never overwrites an earlier artifact")
        if self.directory:
            destination = self.directory / relative
            destination.parent.mkdir(parents=True, exist_ok=True)
            destination.write_bytes(payload)
        self.manifest["artifacts"][relative] = {"path": relative, "bytes": len(payload), "sha256": digest(payload)}

    def retain_json(self, relative, value):
        self.retain(relative, (json.dumps(value, indent=2, allow_nan=False) + "\n").encode("utf-8"))

    def run(self, command, public_command, *, env=None, timeout=60, cwd=ROOT):
        # The caller provides an exact invocation with explicit path placeholders.
        record = {"command": public_command, "started_at": timestamp()}
        self.manifest["commands"].append(record)
        try:
            result = subprocess.run(command, cwd=cwd, capture_output=True, text=True,
                                    errors="replace", env=env, timeout=timeout, check=False)
            record["exit_code"] = result.returncode
            return result
        except subprocess.TimeoutExpired:
            record["exit_code"] = None
            raise Failure("command exceeded its predeclared timeout") from None
        except OSError:
            record["exit_code"] = None
            raise Failure("command could not start") from None
        finally:
            record["finished_at"] = timestamp()

    def finish(self, status, failure=None):
        self.manifest.update(status=status, finished_at=timestamp(), assertions=list(CHECKS))
        if failure:
            self.manifest["failure"] = failure
        if self.directory:
            (self.directory / "manifest.json").write_text(
                json.dumps(self.manifest, indent=2, allow_nan=False) + "\n", encoding="utf-8")


def execute_main(main):
    try:
        return main()
    except Failure as error:
        print("render oracle failed: " + str(error), file=sys.stderr)
        return 1
    except Exception:
        # Never accidentally expose a traceback, user path or native/vendor output.
        print("render oracle failed: unexpected internal error (details withheld from public output)", file=sys.stderr)
        return 1
