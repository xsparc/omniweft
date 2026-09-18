# SPDX-License-Identifier: Apache-2.0
"""Offline room authoring through the public bounded policy client."""
from __future__ import annotations

import argparse
from dataclasses import asdict
import json
from pathlib import Path
import sys
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from omniweft_sdk import (CreateCube, PolicySession, ProtocolError, SetTransform,
                          TemporaryTarget, Transform)

# Implementation inputs, deliberately separate from the independent oracle.
ROOM = (
    ("floor", (2.5,-1.1,0), (0,0,0,1), (2.5,.2,2)),
    ("back", (2.5,.2,-.9), (0,1,0,0), (2.5,2.4,.2)),
    ("side", (1.35,.2,0), (0,0,0,1), (.2,2.4,2)),
    ("table", (2.15,-.55,.35), (0,.6,0,.8), (.8,.9,.7)),
    ("stool", (3.2,-.7,.5), (0,0,0,1), (.4,.6,.5)),
)


def room_steps(client):
    """Yield at each committed/rejected boundary; never retry a mutation."""
    handles = {}
    for number, objects in enumerate((ROOM[:2], ROOM[2:4], ROOM[4:]), 1):
        operations = []
        for name, position, rotation, scale in objects:
            operations.extend((CreateCube(name), SetTransform(TemporaryTarget(name),
                Transform(position, rotation, scale))))
        receipt = client.transact(operations, number-1,
            transaction_id=f"018f7242-4387-7c98-a118-{number:012x}")
        if receipt.status != "committed":
            raise ProtocolError()
        handles.update((binding.temporary_id, binding.handle()) for binding in receipt.created)
        yield "assembled" if number == 3 else "building", receipt
    table = Transform((3.1,-.55,.15), (0,.6,0,.8), (.8,.9,.7))
    rejected = client.transact([
        SetTransform(handles["table"], table),
        SetTransform(handles["stool"], Transform((0,-.7,.5), scale=(.4,.6,.5)))], 3,
        transaction_id="018f7242-4387-7c98-a118-000000000004")
    if rejected.status != "rejected":
        raise ProtocolError()
    yield "rejected", rejected
    recovered = client.transact([
        SetTransform(handles["table"], table),
        SetTransform(handles["stool"], Transform((2,-.7,.5), scale=(.4,.6,.5)))], 3,
        transaction_id="018f7242-4387-7c98-a118-000000000005")
    if recovered.status != "committed":
        raise ProtocolError()
    yield "rearranged", recovered


def wait_runtime(client, predicate):
    deadline = time.monotonic()+8
    while time.monotonic() < deadline:
        value = client.runtime()
        if predicate(value):
            return value
        time.sleep(.025)
    raise ProtocolError()


def idle_policy(client):
    deadline = time.monotonic()+3
    while time.monotonic() < deadline:
        status = client.policy_status()
        if status.usage.requests == 0 and status.usage.global_requests == 0:
            return status
        time.sleep(.01)
    raise ProtocolError()


def run(executable, output, *, gpu=False, interactive=False, inspect=None):
    output = Path(output)
    output.mkdir(parents=True, exist_ok=False)
    records = []
    with PolicySession(executable, gpu=gpu, interactive=interactive,
                       output=output/"native", max_runtime_ms=60000) as session:
        agent, observer = session.client("east"), session.client("east")
        initial = observer.observe().to_dict()
        if initial["world_revision"] != 0:
            raise ProtocolError()
        if gpu:
            wait_runtime(observer, lambda x: x.presentation.ready)
        before_rejection = None
        for phase, receipt in room_steps(agent):
            snapshot = observer.observe().to_dict()
            status = idle_policy(observer)
            runtime = observer.runtime()
            if gpu and phase in ("assembled", "rearranged"):
                runtime = wait_runtime(observer, lambda x: x.presentation.world_revision == snapshot["world_revision"])
            if inspect is not None:
                inspect(phase, receipt, session.client("east"))
            if phase == "assembled":
                before_rejection = snapshot
            if phase == "rejected" and snapshot != before_rejection:
                raise ProtocolError()
            records.append({"phase":phase, "receipt":receipt.to_dict(), "snapshot":snapshot,
                "runtime":runtime.to_dict(), "policy":{"principal":status.principal,
                "world_revision":status.world_revision, "next_sequence":status.next_sequence,
                "limits":asdict(status.limits), "usage":asdict(status.usage),
                "lower":list(status.lower), "upper":list(status.upper)}})
        if interactive:
            while session.is_running:
                time.sleep(.05)
    report = {"schema_version":1,"example":"showcase.ai_blocks","seed":7,
              "lane":"gpu" if gpu else "cpu","initial":initial,"records":records}
    (output/"result.json").write_text(json.dumps(report,indent=2,allow_nan=False)+"\n",encoding="utf-8")
    return report


def main():
    parser=argparse.ArgumentParser(description="Build and rearrange a room through the public scoped SDK.")
    parser.add_argument("--executable",type=Path,required=True)
    parser.add_argument("--output",type=Path,required=True)
    parser.add_argument("--seed",type=int,choices=[7],default=7)
    mode=parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--headless",action="store_true")
    mode.add_argument("--gpu",action="store_true")
    parser.add_argument("--interactive",action="store_true")
    args=parser.parse_args()
    try:
        if args.interactive and not args.gpu:
            raise ProtocolError()
        run(args.executable,args.output,gpu=args.gpu,interactive=args.interactive)
        print("showcase.ai_blocks: room built, invalid plan rejected, rearrangement completed")
        return 0
    except Exception:
        print("SHOWCASE_FAILED: bounded room fixture did not complete",file=sys.stderr)
        return 4


if __name__=="__main__":
    raise SystemExit(main())
