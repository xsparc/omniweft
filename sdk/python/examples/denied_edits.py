#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""The public SDK policy fixture; writes only allowlisted domain observations."""
import argparse
import json
from dataclasses import asdict
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from omniweft_sdk import (ApiError, CreateCube, DeleteEntity, PolicySession, SetTransform,
                          TemporaryTarget, Transform)


def run(executable, output):
    output = Path(output)
    output.mkdir(parents=True, exist_ok=False)
    records = []
    with PolicySession(executable, max_runtime_ms=30000) as session:
        west, east = session.client("west"), session.client("east")
        for principal, client in (("west", west), ("east", east)):
            status = client.policy_status()
            records.append({"kind": "profile", "principal": principal, "limits": asdict(status.limits),
                            "usage": asdict(status.usage), "world_revision": status.world_revision})
        n = 0

        def edit(client, operations, revision):
            nonlocal n
            n += 1
            receipt = client.transact(operations, revision,
                transaction_id=f"018f7242-4387-7c98-a114-{n:012x}")
            records.append({"kind": "receipt", "value": receipt.to_dict()})
            return receipt

        def observe():
            snapshot = east.observe()
            records.append({"kind": "snapshot", "value": snapshot.to_dict()})
            return snapshot

        a = edit(west, [CreateCube("A"), SetTransform(TemporaryTarget("A"), Transform(position_m=(-3,0,0)))], 0)
        b = edit(east, [CreateCube("B"), SetTransform(TemporaryTarget("B"), Transform(position_m=(3,0,0)))], 1)
        west_handle, east_handle = a.created[0].handle(), b.created[0].handle()
        observe()
        edit(west, [SetTransform(east_handle, Transform(position_m=(-3,0,0)))], 2)
        edit(west, [SetTransform(west_handle, Transform(position_m=(-1,0,0)))], 2)
        edit(west, [CreateCube("X"), CreateCube("Y"), DeleteEntity(TemporaryTarget("X")),
                    DeleteEntity(TemporaryTarget("Y"))], 2)
        for _ in range(5):
            edit(west, [SetTransform(west_handle, Transform(position_m=(-4,0,0))),
                        SetTransform(TemporaryTarget("missing"), Transform(position_m=(-3,0,0)))], 2)
        observe()
        for label, action in (
            ("operations", lambda: west.transact([SetTransform(west_handle, Transform(position_m=(-3,0,0)))] * 5, 2)),
            ("observation", west.observe), ("runtime", west.runtime)):
            try:
                action()
                records.append({"kind": "http_rejection", "case": label, "code": "UNEXPECTED_SUCCESS"})
            except ApiError as error:
                records.append({"kind": "http_rejection", "case": label, "code": error.code})
        edit(west, [DeleteEntity(west_handle)], 2)
        edit(east, [CreateCube("C"), SetTransform(TemporaryTarget("C"), Transform(position_m=(5,0,0)))], 3)
        for principal, client in (("west", west), ("east", east)):
            status = client.policy_status()
            records.append({"kind": "usage", "principal": principal, "value": asdict(status.usage)})
        observe()
        session.renew()
        west, east = session.client("west"), session.client("east")
        for principal, client in (("west", west), ("east", east)):
            status = client.resync_policy()
            records.append({"kind": "renewed", "principal": principal, "value": asdict(status.usage),
                            "world_revision": status.world_revision, "next_sequence": status.next_sequence})
        edit(west, [CreateCube("D"), SetTransform(TemporaryTarget("D"), Transform(position_m=(-3,0,0)))], 4)
        observe()
    (output / "policy.json").write_text(json.dumps({"schema_version": 1, "example": "policy.denied_edits",
        "seed": 7, "records": records}, indent=2) + "\n", encoding="utf-8", newline="\n")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--executable", required=True)
    parser.add_argument("--output", required=True)
    args = parser.parse_args()
    try:
        run(args.executable, args.output)
    except Exception:
        print("POLICY_EXAMPLE_FAILED", file=sys.stderr)
        sys.exit(1)
