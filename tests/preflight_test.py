#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Exercise the documented missing-tools path using an empty PATH."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parent.parent
with tempfile.TemporaryDirectory(prefix="omniweft-no-tools-") as empty:
    environment = dict(os.environ, PATH=empty)
    result = subprocess.run([sys.executable, str(ROOT / "tools/bootstrap.py"), "--check"],
                            env=environment, capture_output=True, text=True, timeout=10, check=False)
    if result.returncode != 2:
        raise AssertionError(f"Missing tools should exit 2; got {result.returncode}: {result.stderr}")
    for expected in ("Missing cmake on PATH", "Missing ninja on PATH", "Missing ctest on PATH", "toolchains/build-tools.txt"):
        if expected not in result.stderr:
            raise AssertionError(f"Missing actionable diagnostic {expected}: {result.stderr}")
    compiler = "Missing cl on PATH" if sys.platform == "win32" else "Missing clang++-18 on PATH"
    if compiler not in result.stderr:
        raise AssertionError(f"Missing compiler diagnostic: {result.stderr}")
print("passed: missing build tools and compiler produce install/activation diagnostics")
