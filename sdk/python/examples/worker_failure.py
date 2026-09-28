# SPDX-License-Identifier: Apache-2.0
"""Offline provider lifecycle example through the existing policy retry host."""
from __future__ import annotations
import argparse
from dataclasses import asdict
import json
from pathlib import Path
import sys
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from omniweft_sdk import (CubeGoal, EntityHandle, ProtocolError, RetryPolicySession,
                          WorkerSupervisor)


def wait_status(supervisor, identifier, states, *, cleaned=True):
    end = time.monotonic() + 8
    while time.monotonic() < end:
        value = supervisor.status(identifier)
        if value.state in states and (not cleaned or value.worker_cleaned):
            return value
        time.sleep(.005)
    raise ProtocolError()


def public_status(value):
    return {"request_id": value.request_id, "generation": value.generation,
            "state": value.state, "expected_revision": value.expected_revision,
            "late_results": value.late_results,
            "receipt": None if value.receipt is None else value.receipt.to_dict(),
            "error_code": value.error_code, "worker_cleaned": value.worker_cleaned}


def idle(observer):
    end = time.monotonic() + 3
    while time.monotonic() < end:
        value = observer.policy_status()
        if value.usage.global_requests == 0:
            return value
        time.sleep(.005)
    raise ProtocolError()


def public_policy(value):
    return {"principal": value.principal, "world_revision": value.world_revision,
            "next_sequence": value.next_sequence, "limits": asdict(value.limits),
            "usage": asdict(value.usage), "lower": list(value.lower), "upper": list(value.upper)}


def responsive(supervisor, request, observer):
    first = public_status(supervisor.status(request.request_id))
    idle(observer)
    before = observer.runtime().to_dict()
    time.sleep(.1)
    last = public_status(supervisor.status(request.request_id))
    idle(observer)
    after = observer.runtime().to_dict()
    if (first["state"] != "running" or last["state"] != "running"
            or after["simulation_tick"] <= before["simulation_tick"]
            or before["snapshot"] != after["snapshot"]):
        raise ProtocolError()
    return {"statuses": [first, last], "runtime": [before, after]}


def record(phase, status, observer, progress=None):
    policy = idle(observer)
    snapshot = observer.observe().to_dict()
    return {"phase": phase, "status": public_status(status), "snapshot": snapshot,
            "policy": public_policy(policy), "progress": progress}


def fixture(executable):
    with RetryPolicySession(executable) as host:
        observer = host.client('east')
        client = host.client('east')
        client.capabilities()
        records = []
        goal = CubeGoal.create('A', 3)
        with WorkerSupervisor(client) as supervisor:
            request = supervisor.start(goal, 0, mode='delay', delay_ms=2000, timeout_ms=1500)
            progress = responsive(supervisor, request, observer)
            status = wait_status(supervisor, request.request_id, {'timed_out'})
            records.append(record('timeout', status, observer, progress))

            request = supervisor.start(goal, 0, mode='delay', delay_ms=1000, timeout_ms=4000)
            progress = responsive(supervisor, request, observer)
            if supervisor.cancel(request.request_id) != 'cancelled':
                raise ProtocolError()
            status = wait_status(supervisor, request.request_id, {'cancelled'})
            records.append(record('cancelled', status, observer, progress))

            request = supervisor.start(goal, 0, mode='crash', delay_ms=700, timeout_ms=4000)
            progress = responsive(supervisor, request, observer)
            status = wait_status(supervisor, request.request_id, {'crashed'})
            records.append(record('crashed', status, observer, progress))
            restarted_generation = supervisor.restart()

            old = supervisor.start(goal, 0, mode='delay', delay_ms=1000, timeout_ms=4000)
            progress = responsive(supervisor, old, observer)
            replacement = supervisor.supersede(goal, 0, timeout_ms=5000,
                transaction_id='018f7242-4387-7c98-a115-000000001505')
            status = wait_status(supervisor, old.request_id, {'superseded'})
            old_submit_rejected = False
            try:
                supervisor.submit(old.request_id)
            except ProtocolError:
                old_submit_rejected = True
            records.append(record('superseded', status, observer, progress))
            wait_status(supervisor, replacement.request_id, {'proposal_ready'})
            idle(observer)
            supervisor.submit(replacement.request_id)
            status = wait_status(supervisor, replacement.request_id, {'committed'})
            records.append(record('replacement', status, observer))
            binding = status.receipt.created[0]
            handle = EntityHandle(binding.world_id, binding.entity_uuid, binding.generation)

            final_generation = supervisor.restart()
            request = supervisor.start(CubeGoal.move(handle, 4), 1, timeout_ms=5000,
                transaction_id='018f7242-4387-7c98-a115-000000001506')
            wait_status(supervisor, request.request_id, {'proposal_ready'})
            idle(observer)
            supervisor.submit(request.request_id)
            status = wait_status(supervisor, request.request_id, {'committed'})
            records.append(record('restarted', status, observer))
            retained = [public_status(supervisor.status(i)) for i in range(1, 7)]
        if not old_submit_rejected:
            raise ProtocolError()
    return {"schema_version": 1, "example": "agents.worker_failure", "seed": 7,
            "lane": "cpu", "records": records, "retained": retained,
            "old_submit_rejected": old_submit_rejected,
            "restart_generations": [restarted_generation, final_generation],
            "host_exit_code": host.returncode}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--seed', type=int, choices=[7], default=7)
    parser.add_argument('--headless', action='store_true', required=True)
    parser.add_argument('--verify', action='store_true')
    args = parser.parse_args()
    try:
        if args.output.exists():
            raise ProtocolError()
        report = fixture(args.executable)
        if args.verify and ([r['status']['state'] for r in report['records']] !=
                ['timed_out', 'cancelled', 'crashed', 'superseded', 'committed', 'committed']
                or [r['snapshot']['world_revision'] for r in report['records']] != [0, 0, 0, 0, 1, 2]
                or report['host_exit_code'] != 0):
            raise ProtocolError()
        args.output.mkdir(parents=True)
        (args.output / 'result.json').write_text(json.dumps(report, indent=2, allow_nan=False)+'\n', encoding='utf-8')
        print('agents.worker_failure: bounded lifecycle fixture completed')
        return 0
    except Exception:
        print('WORKER_EXAMPLE_FAILED: fixture did not complete', file=sys.stderr)
        return 4


if __name__ == '__main__':
    raise SystemExit(main())
