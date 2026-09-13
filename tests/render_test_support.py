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
        self.check_start = len(CHECKS)
        self.executable = Path(executable)
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
            self.manifest["environment"]["build"] = self._build_provenance(Path(executable), mode)
            require(not self.directory.exists(), "evidence output is fresh")
            self.directory.mkdir(parents=True)

    def _build_provenance(self, executable, mode):
        build = executable.parent
        cache_path = build / "CMakeCache.txt"
        require(cache_path.is_file(), "retained executable has actual CMake build metadata")
        cache = dict(re.findall(r"^([A-Za-z0-9_-]+):[^=\n]+=(.*)$",
                               cache_path.read_text(encoding="utf-8"), re.MULTILINE))
        require("CMAKE_HOME_DIRECTORY" in cache and
                Path(cache["CMAKE_HOME_DIRECTORY"]).resolve() == ROOT,
                "retained executable CMake source matches the exact oracle checkout")
        require(cache.get("CMAKE_GENERATOR") == "Ninja", "retained build uses the declared Ninja generator")
        configuration = cache.get("CMAKE_BUILD_TYPE")
        require(configuration in ("Debug","Release","RelWithDebInfo","MinSizeRel"), "retained build configuration is known")
        vulkan = cache.get("OW_ENABLE_VULKAN")
        require(vulkan in ("ON","OFF","TRUE","FALSE","1","0"), "retained optional Vulkan setting is known")
        enabled = vulkan in ("ON","TRUE","1")
        require(mode != "gpu" or enabled, "retained GPU executable was configured with Vulkan enabled")
        versions = {}
        for name,key,pattern in (("cmake","CMAKE_COMMAND",r"cmake version ([0-9]+(?:\.[0-9]+)+)"),
                                 ("ninja","CMAKE_MAKE_PROGRAM",r"^([0-9]+(?:\.[0-9]+)+(?:[.+_-][A-Za-z0-9_-]+)*)\s*$")):
            require(key in cache and Path(cache[key]).is_file(), "actual "+name+" executable exists")
            result = self.run([cache[key],"--version"],["<"+name+">","--version"],timeout=10)
            match = re.search(pattern,result.stdout)
            require(result.returncode == 0 and match is not None, "actual "+name+" version is recorded safely")
            versions[name] = match.group(1)
            versions[name+"_executable_sha256"] = digest(Path(cache[key]).read_bytes())
        compiler_info = build / "CMakeFiles" / versions["cmake"] / "CMakeCXXCompiler.cmake"
        require(compiler_info.is_file(), "actual configured compiler metadata exists")
        compiler_text = compiler_info.read_text(encoding="utf-8")
        compiler_id = re.search(r'set\(CMAKE_CXX_COMPILER_ID "([A-Za-z]+)"\)',compiler_text)
        compiler_version = re.search(r'set\(CMAKE_CXX_COMPILER_VERSION "([0-9]+(?:\.[0-9]+)+)"\)',compiler_text)
        require(compiler_id is not None and compiler_id.group(1) in ("MSVC","Clang","GNU","AppleClang") and
                compiler_version is not None, "actual compiler ID and numeric version are recorded safely")
        compiler_path_match = re.search(r'set\(CMAKE_CXX_COMPILER "([^"]+)"\)',compiler_text)
        require(compiler_path_match is not None, "configured compiler has an actual resolved path")
        compiler_path = Path(compiler_path_match.group(1))
        require(compiler_path.is_absolute() and compiler_path.is_file(),
                "actual configured compiler executable exists")
        versions["compiler"] = {"id":compiler_id.group(1),"version":compiler_version.group(1),
                               "executable_sha256":digest(compiler_path.read_bytes())}
        versions.update(configuration=configuration, generator="Ninja", vulkan_enabled=enabled,
                        source_directory="<repo>")
        return versions

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
        if status == "passed":
            require(digest(self.executable.read_bytes()) == self.manifest["executable_sha256"],
                    "tested executable remains byte-for-byte stable throughout the run")
        scoped_checks = list(CHECKS[self.check_start:])
        inconsistent_pass = status == "passed" and any(not check["passed"] for check in scoped_checks)
        if inconsistent_pass:
            status, failure = "failed", "retained evidence contains a failed assertion"
        self.manifest.update(status=status, finished_at=timestamp(), assertions=scoped_checks)
        if failure:
            self.manifest["failure"] = failure
        if self.directory:
            (self.directory / "manifest.json").write_text(
                json.dumps(self.manifest, indent=2, allow_nan=False) + "\n", encoding="utf-8")
        if inconsistent_pass:
            raise Failure(failure)


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

