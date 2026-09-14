#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Compile a real clock-cap mutation; the independent native callback oracle must reject it."""
import argparse
import difflib
from pathlib import Path, PurePosixPath
import re
import shutil
import sys
import tempfile

from agents_test_support import Evidence, Failure, ROOT, digest, main_guard, require

TARGET = "src/simulation.cpp"
ORIGINAL = "constexpr std::uint64_t catch_up_cap = 4;"
MUTANT = "constexpr std::uint64_t catch_up_cap = 5;"
EXPECTED_FAILURE = "simulation oracle failed: clock cap: executed callback count"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--evidence-dir", type=Path, required=True)
    args = parser.parse_args()
    build = args.build_dir.resolve(strict=True)
    name = "simulation_native_test.exe" if sys.platform == "win32" else "simulation_native_test"
    executable = build / name
    evidence = Evidence(args.evidence_dir, executable, proof="deliberate_native_clock_cap_change")
    source_before = (ROOT / TARGET).read_bytes()
    executable_before = executable.read_bytes()
    try:
        cache = dict(re.findall(r"^([A-Za-z0-9_-]+):[^=\n]+=(.*)$",
                                (build / "CMakeCache.txt").read_text("utf-8"), re.MULTILINE))
        require(Path(cache["CMAKE_HOME_DIRECTORY"]).resolve() == ROOT,
                "baseline build belongs to exact tested source")
        baseline = evidence.run([str(executable)], ["<simulation-native-test>"], timeout=10)
        require(baseline.returncode == 0, "original independent native clock oracle passes")
        from agents_test_support import strict_json
        baseline_report = strict_json(baseline.stdout.encode("utf-8"))
        require(type(baseline_report) is dict and baseline_report.keys() == {"status", "case_set", "assertions"}
                and baseline_report["status"] == "passed" and baseline_report["case_set"] == "fixed-clock-v1"
                and type(baseline_report["assertions"]) is int and baseline_report["assertions"] >= 200,
                "original native clock assertion report is bounded and complete")
        evidence.retain_json("baseline-clock.json", baseline_report)
        original_text = source_before.decode("utf-8-sig")
        require(original_text.count(ORIGINAL) == 1, "clock mutation matches exactly one real cap")
        mutant_text = original_text.replace(ORIGINAL, MUTANT)
        require(mutant_text != original_text, "mutation changes the actual native catch-up cap")
        with tempfile.TemporaryDirectory(prefix="ow-agents-clock-mutant-") as temporary:
            source = Path(temporary) / "source"
            target_build = Path(temporary) / "build"
            source.mkdir()
            tracked = evidence.run(["git", "ls-files", "-z"], ["git", "ls-files", "-z"], timeout=10)
            require(tracked.returncode == 0, "enumerate exact tracked candidate sources")
            for relative in tracked.stdout.split("\0"):
                if not relative:
                    continue
                path = PurePosixPath(relative)
                require(not path.is_absolute() and ".." not in path.parts and ".git" not in path.parts,
                        "tracked source remains repository-relative")
                original = ROOT / relative
                require(original.is_file() and not original.is_symlink() and original.resolve().is_relative_to(ROOT),
                        "tracked source stays inside candidate checkout")
                destination = source / relative
                destination.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(original, destination)
            (source / TARGET).write_text(mutant_text, encoding="utf-8", newline="\n")
            evidence.manifest["mutation"] = {
                "source_path": TARGET, "original_source_sha256": digest(source_before),
                "mutated_source_sha256": digest((source / TARGET).read_bytes()),
                "original_executable_sha256": digest(executable_before),
                "expected_oracle_failure": EXPECTED_FAILURE}
            patch = "".join(difflib.unified_diff(original_text.splitlines(keepends=True),
                             mutant_text.splitlines(keepends=True), fromfile="original/" + TARGET, tofile="mutant/" + TARGET))
            evidence.retain("mutation.patch", patch.encode("utf-8"))
            command = [cache["CMAKE_COMMAND"], "-S", str(source), "-B", str(target_build),
                       "-G", "Ninja", "-DBUILD_TESTING=ON"]
            public = ["<cmake>", "-S", "<mutant-source>", "-B", "<mutant-build>", "-G", "Ninja", "-DBUILD_TESTING=ON"]
            for key in sorted(cache):
                if key.startswith("OW_") or key in {"CMAKE_BUILD_TYPE", "CMAKE_C_COMPILER", "CMAKE_CXX_COMPILER",
                                                  "CMAKE_MAKE_PROGRAM", "CMAKE_C_FLAGS", "CMAKE_CXX_FLAGS", "CMAKE_EXE_LINKER_FLAGS"}:
                    value = cache[key]
                    if value.endswith("-NOTFOUND"):
                        continue
                    command.append("-D" + key + "=" + value)
                    shown = value if key in ("OW_ENABLE_VULKAN", "CMAKE_BUILD_TYPE") else "<same-baseline-setting>"
                    public.append("-D" + key + "=" + shown)
            result = evidence.run(command, public, timeout=180)
            require(result.returncode == 0, "isolated clock mutant configures with baseline pinned settings")
            result = evidence.run([cache["CMAKE_COMMAND"], "--build", str(target_build), "--target", "simulation_native_test", "--parallel", "2"],
                                  ["<cmake>", "--build", "<mutant-build>", "--target", "simulation_native_test", "--parallel", "2"],
                                  timeout=300)
            require(result.returncode == 0, "isolated real clock mutant compiles")
            mutated_executable = target_build / name
            evidence.manifest["mutation"]["mutated_executable_sha256"] = digest(mutated_executable.read_bytes())
            result = evidence.run([str(mutated_executable)], ["<mutant-simulation-native-test>"], timeout=10)
            require(result.returncode == 1, "independent callback oracle rejects the real native cap mutation")
            require(EXPECTED_FAILURE in result.stderr, "mutant fails on an actual fifth callback")
            evidence.retain_json("oracle-rejection.json", {"exit_code": 1, "assertion": EXPECTED_FAILURE, "status": "expected_failure"})
        require((ROOT / TARGET).read_bytes() == source_before, "original clock source stays byte-identical")
        require(executable.read_bytes() == executable_before, "original native executable stays byte-identical")
        evidence.finish("passed")
        print('{"status":"passed","proof":"independent oracle detected native catch-up cap violation"}')
        return 0
    except Failure as error:
        evidence.finish("failed", str(error))
        raise
    except Exception:
        evidence.finish("failed", "unexpected internal error; private details withheld")
        raise
    finally:
        if (ROOT / TARGET).read_bytes() != source_before or executable.read_bytes() != executable_before:
            raise Failure("mutation attempt changed protected source or executable")


if __name__ == "__main__":
    raise SystemExit(main_guard(main))
