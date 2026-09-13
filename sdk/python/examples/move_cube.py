# SPDX-License-Identifier: Apache-2.0
"""Run sdk.move_cube from a separate Python process against a native host."""
from __future__ import annotations

import argparse
import json
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from omniweft_sdk import CreateCube, NativeSession, ProtocolError, SetTransform, Transform


def main() -> int:
    parser = argparse.ArgumentParser(description="Run the bounded sdk.move_cube fixture.")
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--seed", type=int, choices=[7], default=7)
    parser.add_argument("--verify", action="store_true")
    args = parser.parse_args()
    try:
        if args.output.exists():
            raise ProtocolError()
        with NativeSession(args.executable, max_slots=8) as host:
            client = host.client()
            client.capabilities()
            initial = client.observe()
            first = client.transact([CreateCube("cube")], initial.world_revision,
                                    transaction_id="018f7242-4387-7c98-a114-67787915a501")
            if first.status != "committed" or len(first.created) != 1:
                raise ProtocolError()
            created = client.observe()
            handle = first.created[0].handle()
            second = client.transact(
                [SetTransform(handle, Transform((2.5, -1.0, 3.0), (0.0, 0.0, 0.0, 1.0), (1.0, 2.0, 1.0)))],
                created.world_revision, transaction_id="018f7242-4387-7c98-a114-67787915a502")
            final = client.observe()
            entity = final.get_entity(handle)
            if args.verify and (initial.world_revision != 0 or first.world_revision != 1
                                or second.status != "committed" or second.world_revision != 2
                                or final.world_revision != 2 or entity.authoring_revision != 2
                                or entity.transform.position_m != (2.5, -1.0, 3.0)
                                or entity.transform.scale != (1.0, 2.0, 1.0)):
                raise ProtocolError()
            report = {"schema_version": 1, "example": "sdk.move_cube", "seed": 7, "status": "passed",
                      "receipts": [first.to_dict(), second.to_dict()],
                      "snapshots": [initial.to_dict(), created.to_dict(), final.to_dict()]}
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.mkdir()
        (args.output / "result.json").write_text(json.dumps(report, indent=2, allow_nan=False) + "\n",
                                                 encoding="utf-8")
        print("sdk.move_cube: typed receipts and snapshots written")
        return 0
    except Exception:
        print("SDK_EXAMPLE_FAILED: fixture did not complete", file=sys.stderr)
        return 4


if __name__ == "__main__":
    raise SystemExit(main())
