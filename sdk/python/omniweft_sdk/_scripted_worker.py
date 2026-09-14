# SPDX-License-Identifier: Apache-2.0
"""Private worker for the fixed seed-7 builder, never a generated-code runner."""
from __future__ import annotations

import json
import os
import threading
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from omniweft_sdk import (Client, ConnectionInfo, CreateCube, ProtocolError,
                          SetTransform, TemporaryTarget, Transform)

# These authoring values are implementation inputs. Independent tests own their
# own fixture and canonical encoder, and do not import this list.
STEPS = (
    ("left", (-2.0, 0.0, 0.0), (0.75, 1.0, 1.0)),
    ("centre", (0.0, 0.5, 0.0), (1.0, 0.5, 1.0)),
    ("right", (2.0, -0.5, 0.0), (0.5, 1.5, 1.0)),
)


def send(event, receipts, snapshots):
    value = {"event": event, "receipts": [x.to_dict() for x in receipts],
             "snapshots": [x.to_dict() for x in snapshots]}
    data = json.dumps(value, separators=(",", ":"), allow_nan=False).encode("utf-8") + b"\n"
    if len(data) > 65536:
        raise ProtocolError()
    sys.stdout.buffer.write(data)
    sys.stdout.buffer.flush()


def barrier(expected):
    value = sys.stdin.buffer.readline(9)
    if value in (b"", b"stop\n"):
        return False
    if value != expected:
        raise ProtocolError()
    return True


def main():
    try:
        descriptor = sys.stdin.buffer.readline(514)
        if len(descriptor) > 513 or not descriptor.endswith(b"\n"):
            raise ProtocolError()
        client = Client(ConnectionInfo.from_descriptor(descriptor[:-1]))
        client.capabilities()
        current = client.observe()
        if current.seed != 7 or current.max_slots != 8 or current.world_revision != 0:
            raise ProtocolError()
        send("ready", [], [current])
        if not barrier(b"build\n"):
            return 0

        def build(index, snapshot):
            name, position, scale = STEPS[index]
            receipt = client.transact(
                [CreateCube(name), SetTransform(TemporaryTarget(name),
                    Transform(position, (0.0, 0.6, 0.0, 0.8), scale))],
                snapshot.world_revision,
                transaction_id="018f7242-4387-7c98-a114-67787915a60" + str(index + 1))
            if receipt.status != "committed":
                raise ProtocolError()
            return receipt, client.observe()

        first, current = build(0, current)
        send("first", [first], [current])
        if not barrier(b"finish\n"):
            return 0
        second, middle = build(1, current)
        third, final = build(2, middle)
        send("done", [second, third], [middle, final])
        return 0
    except Exception:
        # No private descriptors, reflected errors or local paths leave the worker.
        return 4


if __name__ == "__main__":
    # Bounds private barriers and pipe writes even if the owner stops polling.
    # This thread never accesses credentials, the client, or domain state.
    watchdog = threading.Timer(30.0, lambda: os._exit(4))
    watchdog.daemon = True
    watchdog.start()
    try:
        result = main()
    finally:
        watchdog.cancel()
        watchdog.join()
    raise SystemExit(result)
