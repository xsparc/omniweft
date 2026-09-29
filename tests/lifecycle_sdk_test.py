#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Bounded worker/state tests. The separate lifecycle oracle uses a real host."""
from dataclasses import FrozenInstanceError
import copy
import json
from pathlib import Path
import sys
import threading
import time
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "sdk/python"))
from omniweft_sdk import lifecycle
from omniweft_sdk.lifecycle import CubeGoal, WorkerSupervisor
from omniweft_sdk.models import ApiError, EntityHandle, OutcomeUnknown, ProtocolError, Receipt
from omniweft_sdk.retry import PreparedTransaction, RetryPolicyClient

checks = 0
TX = "018f7242-4387-7c98-a114-000000001501"
HANDLE = EntityHandle("workshop", "00000007-0000-4000-8000-000000000001", 1)


class Failure(Exception):
    pass


def check(value, label):
    global checks
    checks += 1
    if not value:
        raise Failure(label)


def rejected(call, kind=ProtocolError):
    try:
        call()
    except kind:
        check(True, "expected rejection")
        return
    raise Failure("invalid state accepted")


def wait(supervisor, identifier, states, *, cleaned=False, timeout=5):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        value = supervisor.status(identifier)
        if value.state in states and (not cleaned or value.worker_cleaned):
            return value
        time.sleep(.005)
    raise Failure("bounded status wait")


class Client(RetryPolicyClient):
    """No sockets: controlled negotiation/response gates exercise supervisor races."""
    def __init__(self):
        self._pending = None
        self._uncertain = False
        self.owner = object()
        self.prepared = []
        self.sent = []
        self.prepare_entered = threading.Event()
        self.prepare_release = threading.Event()
        self.prepare_release.set()
        self.submit_entered = threading.Event()
        self.submit_release = threading.Event()
        self.submit_release.set()
        self.prepare_delay = 0
        self.submit_error = None
        self.reconcile_error = False
        self.receipt_status = "committed"
        self.reconciliations = 0

    def prepare(self, operations, expected_revision, *, transaction_id=None):
        check(self._pending is None, "prepare retains one key")
        self._pending = PreparedTransaction(len(self.prepared) + 1, transaction_id or TX,
            expected_revision, self.owner, b"private prepared body")
        self.prepared.append((self._pending, tuple(operations)))
        self.prepare_entered.set()
        if not self.prepare_release.wait(3):
            raise ProtocolError()
        time.sleep(self.prepare_delay)
        return self._pending

    def submit(self, prepared):
        self.sent.append(prepared)
        self.submit_entered.set()
        if not self.submit_release.wait(3):
            raise ProtocolError()
        if self.submit_error is not None:
            raise self.submit_error
        check(prepared is self._pending, "same prepared identity")
        self._pending = None
        return Receipt(self.receipt_status, "volatile", prepared.transaction_id,
                       prepared.expected_revision + (self.receipt_status == "committed"), (), ())

    def resync_policy(self):
        self.reconciliations += 1
        if self.reconcile_error:
            raise ProtocolError()
        self._pending = None
        self._uncertain = False
        return object()


def ready(supervisor, *, timeout_ms=5000):
    status = supervisor.start(CubeGoal.create("A", 1), 0, timeout_ms=timeout_ms, transaction_id=TX)
    return wait(supervisor, status.request_id, {"proposal_ready"}, cleaned=True)


def inputs_and_success():
    for x in (True, False, float("inf"), float("nan"), 9, -9, 10 ** 1000, "1"):
        rejected(lambda: CubeGoal.create("A", x))
    for name in ("", "1bad", "x" * 33, "../A", True):
        rejected(lambda: CubeGoal.create(name, 0))
    rejected(lambda: CubeGoal.move(EntityHandle("other", HANDLE.entity_uuid, 1), 0))
    check(CubeGoal.move(HANDLE, -8).x == -8, "move lower bound")
    client = Client()
    with WorkerSupervisor(client) as supervisor:
        for options in ({"timeout_ms": 49}, {"timeout_ms": 5001}, {"delay_ms": -1},
                        {"delay_ms": 2001}, {"mode": "arbitrary"}, {"transaction_id": "invalid"}):
            rejected(lambda: supervisor.start(CubeGoal.create("A", 0), 0, **options))
        rejected(lambda: supervisor.start(CubeGoal.create("A", 0), True))
        status = ready(supervisor)
        check(status.request_id == 1 and status.generation == 1, "initial identity")
        check(status.error_code is None and client.sent == [], "proposal has no authority")
        rejected(lambda: setattr(status, "state", "committed"), FrozenInstanceError)
        supervisor.submit(status.request_id)
        result = wait(supervisor, 1, {"committed"})
        check(result.receipt.transaction_id == TX and result.error_code is None, "literal transaction id")
        check(len(client.sent) == 1, "one explicit submit")
        check(supervisor.cancel(1) == "terminal", "committed cancellation")
        check("private prepared body" not in repr(result), "status hides prepared body")
        detached = result.to_dict(); detached["state"] = "forged"
        check(supervisor.status(1).state == "committed", "detached status dict")
        check(supervisor.restart() == 2, "idle restart generation")
    rejected(lambda: supervisor.start(CubeGoal.create("A", 0), 0))


def children_and_late_output():
    for mode, state, code in (("crash", "crashed", "WORKER_CRASHED"),
                              ("malformed", "worker_failed", "INVALID_PROPOSAL"),
                              ("oversized", "worker_failed", "INVALID_PROPOSAL")):
        client = Client()
        with WorkerSupervisor(client) as supervisor:
            request = supervisor.start(CubeGoal.create("A", 1), 0, mode=mode, timeout_ms=4000)
            result = wait(supervisor, request.request_id, {state}, cleaned=True)
            check(result.error_code == code, "fixed child failure code")
            check(result.late_results == 0 and not client.sent, "failed child has no authority")
    with WorkerSupervisor(Client()) as supervisor:
        request = supervisor.start(CubeGoal.create("A", 1), 0, mode="delay", delay_ms=600, timeout_ms=150)
        result = wait(supervisor, request.request_id, {"timed_out"}, cleaned=True)
        check(result.late_results == 0 and result.error_code == "DEADLINE_EXPIRED", "timeout kills child")
        check(supervisor.submit(request.request_id).state == "timed_out", "expired submit stays terminal")
    with WorkerSupervisor(Client()) as supervisor:
        old = supervisor.start(CubeGoal.create("A", 1), 0, mode="delay", delay_ms=400, timeout_ms=4000)
        check(supervisor.cancel(old.request_id) == "cancelled", "cancel before submission")
        result = wait(supervisor, old.request_id, {"cancelled"}, cleaned=True)
        check(result.late_results == 1 and result.error_code == "CANCELLED", "real cancelled late output discarded")
    with WorkerSupervisor(Client()) as supervisor:
        old = supervisor.start(CubeGoal.create("A", 1), 0, mode="delay", delay_ms=500, timeout_ms=4000)
        new = supervisor.supersede(CubeGoal.create("B", 2), 0, mode="delay", delay_ms=300, timeout_ms=4000)
        check((new.request_id, new.generation) == (2, 2), "supersede increments identities")
        rejected(lambda: supervisor.supersede(CubeGoal.create("C", 3), 0))
        wait(supervisor, new.request_id, {"proposal_ready"}, cleaned=True)
        result = wait(supervisor, old.request_id, {"superseded"}, cleaned=True)
        check(result.late_results == 1 and result.generation == 1, "old generation cannot become ready")
        check(supervisor.status(new.request_id).state == "proposal_ready", "replacement unaffected by late result")
    with WorkerSupervisor(Client()) as supervisor:
        with patch.object(lifecycle.subprocess, "Popen", side_effect=OSError("private path")):
            request = supervisor.start(CubeGoal.create("A", 0), 0)
            result = wait(supervisor, request.request_id, {"worker_failed"}, cleaned=True)
        check(result.error_code == "WORKER_FAILED" and "private path" not in repr(result), "startup error redaction")


def correlation_and_deadline():
    with WorkerSupervisor(Client()) as supervisor:
        status = supervisor.start(CubeGoal.create("A", 1), 0, mode="delay", delay_ms=1000, timeout_ms=4000)
        record = supervisor._records[status.request_id]
        literal = {"schema_version": 1, "request_id": 1, "generation": 1, "expected_revision": 0,
                   "operations": [{"type": "entity.create", "temporary_id": "A", "prefab": "builtin.unit_cube"},
                    {"type": "transform.set", "target": {"temporary_id": "A"}, "position_m": [1, 0, 0],
                     "rotation_xyzw": [0, 0, 0, 1], "scale": [1, 1, 1]}]}
        for name in ("request_id", "generation", "expected_revision", "schema_version"):
            for value in (True, literal[name] + 1):
                bad = copy.deepcopy(literal); bad[name] = value
                rejected(lambda: supervisor._accept(record, json.dumps(bad).encode()))
        for name, index, value in (("position_m", 0, True), ("position_m", 1, False),
                                   ("scale", 0, True), ("rotation_xyzw", 3, True)):
            bad = copy.deepcopy(literal); bad["operations"][1][name][index] = value
            rejected(lambda: supervisor._accept(record, json.dumps(bad).encode()))
        bad = copy.deepcopy(literal); bad["operations"][1]["extra"] = 1
        rejected(lambda: supervisor._accept(record, json.dumps(bad).encode()))
        check(supervisor.status(1).state == "running", "forged output preserves pending state")
        supervisor._accept(record, json.dumps(literal).encode())
        check(supervisor.status(1).state == "proposal_ready", "finite integer vector components accepted")
        check(supervisor.cancel(1) == "cancelled", "ready cancellation prevents authority")
    with WorkerSupervisor(Client()) as supervisor:
        status = ready(supervisor, timeout_ms=1000)
        with patch.object(lifecycle.time, "monotonic", return_value=time.monotonic() + 10):
            check(supervisor.submit(status.request_id).state == "timed_out", "ready absolute deadline enforced")



def restart_deadline_boundary():
    for offset, state, code in ((0, "timed_out", "DEADLINE_EXPIRED"),
                                 (1, "timed_out", "DEADLINE_EXPIRED"),
                                 (-.001, "superseded", "WORKER_RESTARTED")):
        client = Client()
        with WorkerSupervisor(client) as supervisor:
            status = ready(supervisor)
            deadline = supervisor._records[status.request_id].deadline
            # No status query at/after expiry before restart; the real child is cleaned.
            with patch.object(lifecycle.time, "monotonic", return_value=deadline + offset):
                check(supervisor.restart() == 2, "restart advances generation at deadline")
                expected = {"request_id": 1, "generation": 1, "state": state,
                    "expected_revision": 0, "late_results": 0, "receipt": None,
                    "error_code": code, "worker_cleaned": True}
                check(supervisor.status(1).to_dict() == expected, "restart preserves deadline outcome")
                if state == "timed_out":
                    check(supervisor.submit(1).to_dict() == expected, "restart terminal cannot submit")
                else:
                    rejected(lambda: supervisor.submit(1))
                check(supervisor.cancel(1) == "terminal", "restart terminal cannot cancel")
                check(supervisor.restart() == 3 and supervisor.status(1).to_dict() == expected,
                      "repeated restart preserves terminal identity")
                check(not client.prepared and not client.sent, "expired or superseded proposal has no authority")
            fresh = ready(supervisor)
            check((fresh.request_id, fresh.generation) == (2, 3), "fresh proposal after restart")
            supervisor.submit(fresh.request_id)
            result = wait(supervisor, fresh.request_id, {"committed"})
            check(result.receipt.world_revision == 1 and len(client.sent) == 1,
                  "restart recovery commits exactly once")


def submit_races_and_recovery():
    client = Client(); client.submit_release.clear(); client.submit_error = OSError("private transport")
    with WorkerSupervisor(client) as supervisor:
        status = ready(supervisor); supervisor.submit(status.request_id)
        check(client.submit_entered.wait(2), "actual submit reached")
        start = time.monotonic()
        check(supervisor.status(1).state == "submitting", "status during blocked I/O")
        check(supervisor.cancel(1) == "too_late", "cancel after submit claim")
        check(time.monotonic() - start < .25, "status cancellation responsiveness")
        rejected(lambda: supervisor.start(CubeGoal.create("B", 2), 0), OutcomeUnknown)
        rejected(lambda: supervisor.supersede(CubeGoal.create("B", 2), 0), OutcomeUnknown)
        original = client.sent[0]
        with patch.object(lifecycle.time, "monotonic", return_value=supervisor._records[1].deadline + 1):
            check(supervisor.restart() == 2, "submitting restart advances generation")
            check(supervisor.status(1).state == "submitting" and supervisor._records[1].prepared is original,
                  "expired proposal deadline cannot erase submitting authority")
            rejected(lambda: supervisor.start(CubeGoal.create("B", 2), 0), OutcomeUnknown)
        client.submit_release.set()
        check(wait(supervisor, 1, {"outcome_unknown"}).error_code == "OUTCOME_UNKNOWN", "unknown response fixed code")
        original = client.sent[0]
        with patch.object(lifecycle.time, "monotonic", return_value=supervisor._records[1].deadline + 1):
            check(supervisor.restart() == 3 and supervisor.status(1).generation == 1, "restart preserves request generation")
            check(supervisor.status(1).state == "outcome_unknown" and supervisor._records[1].prepared is original,
                  "expired proposal deadline cannot erase uncertain authority")
            rejected(lambda: supervisor.start(CubeGoal.create("B", 2), 0), OutcomeUnknown)
        client.submit_error = None
        supervisor.retry(1)
        check(wait(supervisor, 1, {"committed"}).error_code is None, "exact retry recovery")
        check(client.sent == [original, original] and client.sent[1] is original, "same object retried")
        check(len(client.prepared) == 1, "retry never prepares replacement")
    client = Client(); client.receipt_status = "rejected"
    with WorkerSupervisor(client) as supervisor:
        ready(supervisor); supervisor.submit(1)
        check(wait(supervisor, 1, {"rejected"}).receipt.status == "rejected", "typed rejection retained")
        check(supervisor.start(CubeGoal.create("B", 2), 0).request_id == 2, "rejected receipt frees authority")
    client = Client(); client.submit_error = ApiError("REQUIRES_RESYNC", 409)
    with WorkerSupervisor(client) as supervisor:
        ready(supervisor); supervisor.submit(1)
        check(wait(supervisor, 1, {"outcome_unknown"}).error_code == "REQUIRES_RESYNC", "receipt expiry surfaced")
        check(client._pending is client.sent[0], "API error retains exact key")
        rejected(lambda: supervisor.start(CubeGoal.create("B", 2), 0), OutcomeUnknown)
        client.reconcile_error = True; supervisor.reconcile(1)
        check(wait(supervisor, 1, {"outcome_unknown"}).error_code == "RECONCILIATION_FAILED", "failed reconciliation stays unknown")
        rejected(lambda: supervisor.start(CubeGoal.create("B", 2), 0), OutcomeUnknown)
        client.reconcile_error = False; supervisor.reconcile(1)
        check(wait(supervisor, 1, {"reconciled"}).error_code is None, "explicit reconciliation success")
        check(client._pending is None and len(client.sent) == 1, "reconciliation cannot replay")
        check(supervisor.start(CubeGoal.create("B", 2), 0).request_id == 2, "new work after explicit reconcile")


def prepare_deadline_and_close():
    client = Client(); client.prepare_release.clear()
    with WorkerSupervisor(client) as supervisor:
        ready(supervisor, timeout_ms=1000); supervisor.submit(1)
        check(client.prepare_entered.wait(2), "prepare reservation reached")
        check(supervisor.cancel(1) == "too_late", "negotiation claim prevents false cancellation")
        # The controlled client has reserved the key but has not submitted any body.
        time.sleep(1.05); client.prepare_release.set()
        result = wait(supervisor, 1, {"outcome_unknown"})
        check(result.error_code == "DEADLINE_EXPIRED" and not client.sent, "reserved unsent deadline fence")
        check(supervisor.restart() == 2, "unsent reservation restart generation")
        result = supervisor.status(1)
        check(result.state == "outcome_unknown" and result.error_code == "DEADLINE_EXPIRED"
              and supervisor._records[1].prepared is client._pending, "restart retains reconciliation-only reservation")
        rejected(lambda: supervisor.retry(1))
        rejected(lambda: supervisor.start(CubeGoal.create("B", 2), 0), OutcomeUnknown)
        supervisor.reconcile(1)
        check(wait(supervisor, 1, {"reconciled"}).error_code is None, "unsent reservation explicitly cleared")
        check(not client.sent and client._pending is None, "no late first POST")
    client = Client(); client.prepare_release.clear(); client.submit_error = OSError("response lost")
    supervisor = WorkerSupervisor(client)
    closer = None
    try:
        ready(supervisor); supervisor.submit(1)
        check(client.prepare_entered.wait(2), "close race negotiation entered")
        finished = threading.Event(); failures = []
        def close_owned():
            try:
                supervisor.close()
            except Exception:
                failures.append(True)
            finally:
                finished.set()
        closer = threading.Thread(target=close_owned, daemon=False); closer.start()
        time.sleep(.03)
        check(supervisor.status(1).state == "submitting", "close preserves already claimed state")
        check(supervisor.cancel(1) == "too_late", "close never labels submitting cancelled")
        client.prepare_release.set()
        check(finished.wait(3) and not failures, "close reaps negotiation thread")
        check(supervisor.status(1).state == "outcome_unknown", "close preserves uncertain outcome")
        check(supervisor._records[1].prepared is client._pending, "close retains unresolved prepared identity")
    finally:
        client.prepare_release.set()
        if closer is not None:
            closer.join(4)
        supervisor.close()


def capacity_and_cleanup():
    with WorkerSupervisor(Client()) as supervisor:
        for identifier in range(1, 9):
            status = ready(supervisor)
            check(status.request_id == identifier, "monotonic retained request id")
            check(supervisor.cancel(identifier) == "cancelled", "cancel retained request")
        rejected(lambda: supervisor.start(CubeGoal.create("B", 2), 0))
        check(supervisor.status(1).state == "cancelled" and supervisor.status(8).worker_cleaned,
              "capacity cannot evict earlier status")
        rejected(lambda: supervisor.status(True))
    check(not any(t.name.startswith("omniweft-") for t in threading.enumerate()), "all owned threads reaped")


def main():
    inputs_and_success()
    children_and_late_output()
    correlation_and_deadline()
    restart_deadline_boundary()
    submit_races_and_recovery()
    prepare_deadline_and_close()
    capacity_and_cleanup()
    print(json.dumps({"status": "passed", "assertions": checks}, separators=(",", ":")))


if __name__ == "__main__":
    try:
        main()
    except Failure as failure:
        print("lifecycle SDK invariant failed: " + str(failure), file=sys.stderr)
        raise SystemExit(1) from None
    except Exception:
        print("lifecycle SDK invariant failed: unexpected exception", file=sys.stderr)
        raise SystemExit(1) from None
