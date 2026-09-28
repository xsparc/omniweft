# SPDX-License-Identifier: Apache-2.0
"""Fixed, credential-free proposal fixture. It never opens a network connection."""
import json
import os
from pathlib import Path
import sys
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from omniweft_sdk.models import EntityHandle, decode, fields, integer
from omniweft_sdk.lifecycle import CubeGoal


def main():
    raw = sys.stdin.buffer.readline(16385)
    if len(raw) > 16384 or not raw.endswith(b"\n") or sys.stdin.buffer.read(1):
        return 2
    value = fields(decode(raw, 16384), {"schema_version", "request_id", "generation", "expected_revision", "goal", "mode", "delay_ms"})
    integer(value["schema_version"], 1, 1)
    integer(value["request_id"], 1, 8); integer(value["generation"], 1); integer(value["expected_revision"])
    integer(value["delay_ms"], 0, 2000)
    mode = value["mode"]
    if mode not in ("normal", "delay", "crash", "malformed", "oversized"):
        return 2
    goal = fields(value["goal"], {"kind", "x", "name", "handle"})
    handle = None if goal["handle"] is None else EntityHandle(**fields(goal["handle"], {"world_id", "entity_uuid", "generation"}))
    goal = CubeGoal(goal["kind"], goal["x"], goal["name"], handle)
    time.sleep(value["delay_ms"] / 1000)
    if mode == "crash":
        os._exit(23)
    if mode == "malformed":
        sys.stdout.buffer.write(b'{"invalid":\n')
    elif mode == "oversized":
        sys.stdout.buffer.write(b'x' * 16385 + b'\n')
    else:
        out = {name: value[name] for name in ("schema_version", "request_id", "generation", "expected_revision")}
        out["operations"] = [operation.to_dict() for operation in goal._operations()]
        sys.stdout.buffer.write(json.dumps(out, allow_nan=False, separators=(",", ":")).encode("utf-8") + b'\n')
    sys.stdout.buffer.flush()
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception:
        raise SystemExit(2) from None
