#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Compile one disposable native auth bypass; require independent unauthorized-state detection."""
import argparse
import difflib
from pathlib import Path, PurePosixPath
import re
import shutil
import sys
import tempfile

from sdk_test_support import Evidence, Failure, ROOT, digest, main_guard, require

TARGET = "src/control.cpp"
ORIGINAL = "      if (!authorized(request))"
MUTANT = "      if (false)"
EXPECTED_FAILURE = "wrong-token: complete world unchanged"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--evidence-dir", type=Path, required=True)
    args = parser.parse_args()
    build = args.build_dir.resolve(strict=True)
    name = "omniweft_control.exe" if sys.platform == "win32" else "omniweft_control"
    executable = build / name
    evidence = Evidence(args.evidence_dir, executable, "deliberate_native_auth_bypass")
    source_before = (ROOT / TARGET).read_bytes()
    executable_before = executable.read_bytes()
    try:
        cache = dict(re.findall(r"^([A-Za-z0-9_-]+):[^=\n]+=(.*)$",
                                (build / "CMakeCache.txt").read_text("utf-8"), re.MULTILINE))
        require(Path(cache["CMAKE_HOME_DIRECTORY"]).resolve() == ROOT,
                "baseline build belongs to exact tested source")
        original_text = source_before.decode("utf-8-sig")
        require(original_text.count(ORIGINAL) == 1, "auth mutation matches exactly one real guard")
        mutant_text = original_text.replace(ORIGINAL, MUTANT)
        require(mutant_text != original_text, "mutation disables actual native authorization")
        with tempfile.TemporaryDirectory(prefix="ow-sdk-mutant-") as temporary:
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
                       "-G", "Ninja", "-DBUILD_TESTING=OFF"]
            public = ["<cmake>", "-S", "<mutant-source>", "-B", "<mutant-build>", "-G", "Ninja", "-DBUILD_TESTING=OFF"]
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
            require(result.returncode == 0, "isolated auth mutant configures with baseline pinned settings")
            result = evidence.run([cache["CMAKE_COMMAND"], "--build", str(target_build), "--target", "omniweft_control", "--parallel", "2"],
                                  ["<cmake>", "--build", "<mutant-build>", "--target", "omniweft_control", "--parallel", "2"],
                                  timeout=300)
            require(result.returncode == 0, "isolated real auth mutant compiles")
            mutated_executable = target_build / name
            evidence.manifest["mutation"]["mutated_executable_sha256"] = digest(mutated_executable.read_bytes())
            result = evidence.run([sys.executable, str(ROOT / "tests/sdk_oracle.py"), "--executable", str(mutated_executable),
                                   "--case-set", "auth"],
                                  ["<python>", "tests/sdk_oracle.py", "--executable", "<mutant-executable>", "--case-set", "auth"],
                                  timeout=25)
            require(result.returncode == 1, "independent oracle rejects real native authorization bypass")
            require(EXPECTED_FAILURE in result.stderr, "mutant fails on unauthorized complete world mutation")
            evidence.retain_json("oracle-rejection.json", {"exit_code": 1, "assertion": EXPECTED_FAILURE, "status": "expected_failure"})
        require((ROOT / TARGET).read_bytes() == source_before, "original authorization source stays byte-identical")
        require(executable.read_bytes() == executable_before, "original native executable stays byte-identical")
        evidence.finish("passed")
        print('{"status":"passed","proof":"independent oracle detected native unauthorized world mutation"}')
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
