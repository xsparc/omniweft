# SPDX-License-Identifier: Apache-2.0
"""Run the fixed-step native host and a separate seed-7 scripted provider."""
from __future__ import annotations

import argparse
import json
from pathlib import Path
import sys
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from omniweft_sdk import NativeSession, ProtocolError, ScriptedBuilder


def wait_runtime(client, predicate, seconds=8):
    deadline = time.monotonic() + seconds
    while time.monotonic() < deadline:
        value = client.runtime()
        if predicate(value):
            return value
        time.sleep(0.01)
    raise ProtocolError()


def main():
    parser = argparse.ArgumentParser(description="Run agents.mock_builder with a real separate provider.")
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--seed", type=int, choices=[7], default=7)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--headless", action="store_true")
    mode.add_argument("--gpu", action="store_true")
    parser.add_argument("--interactive", action="store_true")
    parser.add_argument("--verify", action="store_true")
    args = parser.parse_args()
    try:
        if args.output.exists() or (args.interactive and not args.gpu):
            raise ProtocolError()
        args.output.mkdir(parents=True)
        with NativeSession(args.executable, max_slots=8, gpu=args.gpu,
                           interactive=args.interactive, output=args.output / "native") as host:
            observer = host.client()
            if args.gpu:
                wait_runtime(observer, lambda x: x.presentation.ready)
            with ScriptedBuilder(host) as provider:
                initial = provider.initial.snapshots[0]
                before = observer.runtime()
                time.sleep(0.25)
                paused = observer.runtime()
                if (paused.simulation_tick <= before.simulation_tick
                        or paused.snapshot.to_dict() != before.snapshot.to_dict()):
                    raise ProtocolError()
                first = provider.create_first()
                if args.gpu:
                    wait_runtime(observer, lambda x: x.presentation.world_revision == 1)
                middle_before = observer.runtime()
                time.sleep(0.25)
                middle_paused = observer.runtime()
                if (middle_paused.simulation_tick <= middle_before.simulation_tick
                        or middle_paused.snapshot.to_dict() != middle_before.snapshot.to_dict()):
                    raise ProtocolError()
                final = provider.finish()
                completed = wait_runtime(observer, lambda x: x.snapshot.world_revision == 3)
                if args.gpu:
                    completed = wait_runtime(observer, lambda x: x.presentation.world_revision == 3)
                snapshots = (initial, *first.snapshots, *final.snapshots)
                receipts = (*first.receipts, *final.receipts)
                if args.verify and ([x.world_revision for x in snapshots] != [0, 1, 2, 3]
                                    or [x.world_revision for x in receipts] != [1, 2, 3]):
                    raise ProtocolError()
            if args.interactive:
                while host.is_running:
                    time.sleep(0.05)
            report = {
                "schema_version": 1, "example": "agents.mock_builder", "seed": 7,
                "status": "passed", "lane": "gpu" if args.gpu else "cpu",
                "receipts": [x.to_dict() for x in receipts],
                "snapshots": [x.to_dict() for x in snapshots],
                "runtime_observations": [x.to_dict() for x in
                    (before, paused, middle_before, middle_paused, completed)],
            }
        (args.output / "result.json").write_text(
            json.dumps(report, indent=2, allow_nan=False) + "\n", encoding="utf-8")
        print("agents.mock_builder: live fixed steps and typed provider results written")
        return 0
    except Exception:
        print("AGENTS_EXAMPLE_FAILED: fixture did not complete", file=sys.stderr)
        return 4


if __name__ == "__main__":
    raise SystemExit(main())
