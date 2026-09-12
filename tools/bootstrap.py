#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Check the pinned tools, then configure/build/test the offline headless shell."""
import argparse
import json
import platform
from pathlib import Path
import re
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parent.parent
PINS = json.loads((ROOT / "toolchains/bootstrap.json").read_text(encoding="utf-8"))


def preflight():
    errors = []
    observed = {"python": platform.python_version(), "platform": platform.platform()}
    python_versions = [PINS["python"], *PINS["python_local_alternates"]]
    if observed["python"] not in python_versions:
        errors.append(f"Python pin mismatch: use {python_versions}; found {observed['python']}. Select the pinned interpreter.")
    system = platform.system()
    if system not in ("Windows", "Linux") or platform.machine().lower() not in ("amd64", "x86_64"):
        errors.append("Use Windows or Linux on x86_64; other bootstrap targets are not validated.")
        return errors, observed
    target = "windows" if system == "Windows" else "linux"
    compiler = PINS["compilers"][target]
    checks = [
        ("cmake", "cmake", ["--version"], r"cmake version ([\d.]+)", PINS["cmake"]),
        ("ninja", "ninja", ["--version"], r"^(\S+)$", PINS["ninja_executable_version"]),
        ("ctest", "ctest", ["--version"], r"ctest version ([\d.]+)", PINS["cmake"]),
        ("compiler", compiler["executable"], ["/Bv"] if target == "windows" else ["--version"],
         r"Version ([\d.]+) for x64" if target == "windows" else r"clang version ([\d.]+)", compiler["version"]),
    ]
    for name, executable, args, pattern, expected in checks:
        path = shutil.which(executable)
        if path is None:
            hint = ("Activate an x64 MSVC developer shell with toolset 14.44.35207."
                    if name == "compiler" and target == "windows" else
                    "Install/select clang++-18." if name == "compiler" else
                    "Install toolchains/build-tools.txt with pip --require-hashes --only-binary=:all: --no-deps.")
            errors.append(f"Missing {executable} on PATH. {hint}")
            continue
        try:
            result = subprocess.run([path, *args], capture_output=True, text=True, timeout=15, check=False)
            match = re.search(pattern, (result.stdout + result.stderr).strip(), re.MULTILINE)
        except (OSError, subprocess.TimeoutExpired) as error:
            errors.append(f"Cannot execute {executable}: {error}")
            continue
        version = match.group(1) if match else "unrecognized"
        observed[name] = {"path": path, "version": version}
        # cl /Bv returns 2 without a source file, but still identifies the compiler.
        allowed_codes = (0, 2) if name == "compiler" and target == "windows" else (0,)
        versions = [expected, *compiler.get("local_alternates", [])] if name == "compiler" else [expected]
        if result.returncode not in allowed_codes or version not in versions:
            errors.append(f"{executable} pin mismatch: require {versions}, found {version} (exit {result.returncode}). Select the pinned tool or obtain review for a servicing-pin update.")
    return errors, observed


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true", help="Report tool availability without configuring or building")
    args = parser.parse_args()
    errors, observed = preflight()
    print(json.dumps(observed, indent=2), flush=True)
    if errors:
        for error in errors:
            print(f"toolchain_error: {error}", file=sys.stderr)
        return 2
    if args.check:
        return 0
    preset = "windows-headless" if platform.system() == "Windows" else "linux-headless"
    commands = [
        ["cmake", "--preset", preset, f"-DPython3_EXECUTABLE={sys.executable}"],
        ["cmake", "--build", "--preset", preset],
        ["ctest", "--preset", preset],
    ]
    for command in commands:
        print("Running: " + " ".join(command), flush=True)
        result = subprocess.run(command, cwd=ROOT, check=False)
        if result.returncode:
            return result.returncode
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
