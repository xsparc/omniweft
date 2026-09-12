#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Independent black-box bootstrap oracle; never imports native fixture logic."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile

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
    if not condition:
        raise AssertionError(message)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", required=True, type=Path)
    args = parser.parse_args()
    executable = args.executable.resolve(strict=True)
    base = ["--example", "platform.bootstrap", "--headless", "--seed", "7", "--verify"]
    checks = []

    def run(argv, code, diagnostic=None):
        result = subprocess.run([str(executable), *argv], capture_output=True, text=True, timeout=5, check=False)
        require(result.returncode == code, f"{argv}: expected exit {code}, got {result.returncode}: {result.stderr}")
        if diagnostic:
            require(diagnostic in result.stderr, f"{argv}: missing actionable diagnostic {diagnostic}: {result.stderr}")
        return result

    with tempfile.TemporaryDirectory(prefix="omniweft-bootstrap-") as temp:
        root = Path(temp)
        first = root / "success with spaces"
        run([*base, "--output", str(first)], 0)
        payload = (first / "result.json").read_bytes()
        require(json.loads(payload) == EXPECTED, "independent expected lifecycle/fixture does not match result")
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
