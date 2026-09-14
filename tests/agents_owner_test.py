#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Compile the actual Owner under controlled time and verify admission boundaries."""
import argparse
import difflib
from pathlib import Path, PurePosixPath
import re
import sys
import tempfile

SCRIPT_ROOT = Path(__file__).resolve().parents[1]
TARGET = "src/agents_main.cpp"
HARNESS = "tests/agents_owner_native_test.cpp"
CLOCK = "using Clock = std::chrono::steady_clock;"
CONTROLLED_CLOCK = "using Clock = OwnerTestClock;"
ADMISSION = "if(pending_->authoring && (!tick || overloaded)) return {};"
CMAKE_APPEND = """
# Disposable test target: actual Owner, controlled clock, production libraries.
add_executable(agents_owner_native_test tests/agents_owner_native_test.cpp)
target_link_libraries(agents_owner_native_test PRIVATE ow_control ow_transactions ow_presentation ow_simulation Threads::Threads)
target_include_directories(agents_owner_native_test SYSTEM PRIVATE third_party)
ow_native_policy(agents_owner_native_test)
"""


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--evidence-dir", type=Path)
    parser.add_argument("--repository", type=Path, help="development only; prohibits retained evidence")
    parser.add_argument("--admission-mutation", choices=("zero", "overload"),
                        help="development only; require the same oracle to reject a broken guard")
    args = parser.parse_args()
    repository = args.repository.resolve(strict=True) if args.repository is not None else SCRIPT_ROOT
    sys.path.insert(0, str(repository / "tests"))
    from agents_test_support import Evidence, Failure, digest, require, strict_json
    require(not (args.evidence_dir and (args.repository is not None or args.admission_mutation)),
            "development owner-test overrides prohibit retained candidate evidence")
    build = args.build_dir.resolve(strict=True)
    executable = build / ("omniweft_agents.exe" if sys.platform == "win32" else "omniweft_agents")
    source_before = (repository / TARGET).read_bytes()
    executable_before = executable.read_bytes()
    harness_before = (SCRIPT_ROOT / HARNESS).read_bytes()
    evidence = Evidence(args.evidence_dir, executable, proof="actual_owner_dispatch_boundaries")
    evidence.manifest["development_override"] = args.repository is not None or args.admission_mutation is not None
    protected = {}
    try:
        cache = dict(re.findall(r"^([A-Za-z0-9_-]+):[^=\n]+=(.*)$",
                                (build / "CMakeCache.txt").read_text("utf-8"), re.MULTILINE))
        require(Path(cache["CMAKE_HOME_DIRECTORY"]).resolve() == repository,
                "baseline owner build belongs to the exact tested source")
        require(cache.get("CMAKE_GENERATOR") == "Ninja" and
                cache.get("OW_ENABLE_VULKAN") in ("OFF", "FALSE", "0"),
                "owner boundary proof uses the pinned headless Ninja baseline")
        original_text = source_before.decode("utf-8-sig")
        require(original_text.count(CLOCK) == 1 and CONTROLLED_CLOCK not in original_text,
                "owner test seam replaces exactly one production clock alias")
        controlled_text = original_text.replace(CLOCK, CONTROLLED_CLOCK)
        if args.admission_mutation:
            require(controlled_text.count(ADMISSION) == 1, "development mutation matches one real admission guard")
            replacement = ("static_cast<void>(tick); if(pending_->authoring && overloaded) return {};" if args.admission_mutation == "zero"
                           else "static_cast<void>(overloaded); if(pending_->authoring && !tick) return {};")
            controlled_text = controlled_text.replace(ADMISSION, replacement)
        with tempfile.TemporaryDirectory(prefix="ow-agents-owner-boundary-") as temporary:
            source, target_build = Path(temporary) / "source", Path(temporary) / "build"
            source.mkdir()
            indexed = evidence.run(["git", "ls-files", "-z"], ["git", "ls-files", "-z"],
                                   cwd=repository, timeout=10)
            require(indexed.returncode == 0, "enumerate exact indexed owner candidate sources")
            tracked = [relative for relative in indexed.stdout.split("\0") if relative]
            require(TARGET in tracked and "CMakeLists.txt" in tracked,
                    "actual owner and build configuration belong to the candidate index")
            if args.evidence_dir:
                require(HARNESS in tracked and "tests/agents_owner_test.py" in tracked,
                        "retained owner proof includes both test sources in the candidate index")
            for relative in tracked:
                path = PurePosixPath(relative)
                original, copied = repository / relative, source / relative
                require(not path.is_absolute() and ".." not in path.parts and ".git" not in path.parts and
                        original.is_file() and not original.is_symlink() and
                        original.resolve().is_relative_to(repository) and copied.resolve().is_relative_to(source),
                        "indexed owner-test copy stays within declared roots")
                raw = original.read_bytes()
                protected[relative] = digest(raw)
                copied.parent.mkdir(parents=True, exist_ok=True)
                copied.write_bytes(raw)
            (source / TARGET).write_text(controlled_text, encoding="utf-8", newline="\n")
            (source / HARNESS).write_bytes(harness_before)
            cmake_original = (source / "CMakeLists.txt").read_text("utf-8-sig")
            require("agents_owner_native_test" not in cmake_original,
                    "owner test target exists only in the disposable build")
            cmake_test = cmake_original + CMAKE_APPEND
            (source / "CMakeLists.txt").write_text(cmake_test, encoding="utf-8", newline="\n")
            evidence.manifest["owner_test_seam"] = {
                "original_source_sha256": digest(source_before),
                "controlled_source_sha256": digest((source / TARGET).read_bytes()),
                "harness_sha256": digest(harness_before),
                "original_executable_sha256": digest(executable_before),
                "time_model": "controlled_owner_steady_time_points_with_real_waits_and_watchdog"}
            for name, before, after, relative in (
                    ("owner-test-seam.patch", original_text, controlled_text, TARGET),
                    ("owner-test-cmake.patch", cmake_original, cmake_test, "CMakeLists.txt")):
                patch = "".join(difflib.unified_diff(before.splitlines(keepends=True), after.splitlines(keepends=True),
                                                    fromfile="original/" + relative, tofile="test/" + relative))
                evidence.retain(name, patch.encode("utf-8"))
            command = [cache["CMAKE_COMMAND"], "-S", str(source), "-B", str(target_build),
                       "-G", "Ninja", "-DBUILD_TESTING=OFF"]
            public = ["<cmake>", "-S", "<owner-test-source>", "-B", "<owner-test-build>",
                      "-G", "Ninja", "-DBUILD_TESTING=OFF"]
            for key in sorted(cache):
                if key.startswith("OW_") or key in {
                    "CMAKE_BUILD_TYPE", "CMAKE_CXX_COMPILER", "CMAKE_MAKE_PROGRAM",
                    "CMAKE_C_FLAGS", "CMAKE_CXX_FLAGS", "CMAKE_EXE_LINKER_FLAGS"}:
                    value = cache[key]
                    if value.endswith("-NOTFOUND"):
                        continue
                    command.append("-D" + key + "=" + value)
                    shown = value if key in ("OW_ENABLE_VULKAN", "CMAKE_BUILD_TYPE") else "<same-baseline-setting>"
                    public.append("-D" + key + "=" + shown)
            configured = evidence.run(command, public, timeout=180)
            require(configured.returncode == 0, "isolated actual-owner harness configures with pinned settings")
            compiled = evidence.run(
                [cache["CMAKE_COMMAND"], "--build", str(target_build), "--target", "agents_owner_native_test", "--parallel", "2"],
                ["<cmake>", "--build", "<owner-test-build>", "--target", "agents_owner_native_test", "--parallel", "2"], timeout=300)
            require(compiled.returncode == 0, "isolated actual-owner harness compiles")
            owner_executable = target_build / ("agents_owner_native_test.exe" if sys.platform == "win32" else "agents_owner_native_test")
            evidence.manifest["owner_test_seam"]["tested_executable_sha256"] = digest(owner_executable.read_bytes())
            result = evidence.run([str(owner_executable)], ["<actual-owner-boundary-test>"], timeout=12)
            require(len(result.stdout.encode("utf-8")) <= 1024 and not result.stderr,
                    "native owner proof emits only bounded fixed JSON")
            report = strict_json(result.stdout.encode("utf-8"))
            require(type(report) is dict and report.get("case_set") == "owner-dispatch-v1" and
                    report.get("status") in ("passed", "failed"), "native owner proof has a known case set and status")
            if report["status"] == "passed":
                require(report.keys() == {"status", "case_set", "assertions"} and
                        type(report["assertions"]) is int and 30 <= report["assertions"] <= 100,
                        "native owner proof contains the complete bounded assertion count")
            else:
                require(report.keys() == {"status", "case_set", "assertion"} and
                        type(report["assertion"]) is str and len(report["assertion"]) <= 128 and
                        re.fullmatch(r"[A-Za-z0-9 ._-]+", report["assertion"]),
                        "failed owner proof exposes only a fixed bounded assertion label")
            evidence.retain_json("owner-dispatch.json", report)
            import json
            print(json.dumps(report, separators=(",", ":")))
            require(result.returncode == 0 and report["status"] == "passed",
                    "actual owner enforces frozen-time overload and cancellation boundaries")
        require(all(digest((repository / path).read_bytes()) == sha for path, sha in protected.items()),
                "indexed original owner candidate files remain byte-identical")
        require(executable.read_bytes() == executable_before and (SCRIPT_ROOT / HARNESS).read_bytes() == harness_before,
                "original agents executable and owner harness remain byte-identical")
        evidence.finish("passed")
        return 0
    except Failure:
        evidence.finish("failed", "actual-owner boundary assertion failed; private details withheld")
        raise
    except Exception:
        evidence.finish("failed", "owner-test setup or execution failed; private details withheld")
        raise
    finally:
        if ((repository / TARGET).read_bytes() != source_before or executable.read_bytes() != executable_before or
                (SCRIPT_ROOT / HARNESS).read_bytes() != harness_before or
                any(digest((repository / path).read_bytes()) != sha for path, sha in protected.items())):
            raise Failure("owner proof changed protected candidate source or executable")


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception:
        print('{"status":"failed","error":"OWNER_BOUNDARY_PROOF_FAILED"}', file=sys.stderr)
        raise SystemExit(1)