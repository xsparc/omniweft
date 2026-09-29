# SPDX-License-Identifier: Apache-2.0
"""Bounded proposal workers; only the owning parent can submit typed mutations."""
from __future__ import annotations

from dataclasses import dataclass, field
import json
import math
import os
from pathlib import Path
import re
import subprocess
import sys
import threading
import time

from .models import (ApiError, CreateCube, EntityHandle, OutcomeUnknown, ProtocolError,
                     Receipt, SetTransform, TemporaryTarget, Transform, decode, fields,
                     integer, uuid)
from .retry import PreparedTransaction, RetryPolicyClient

_FRAME_LIMIT = 16384
_MODES = frozenset({"normal", "delay", "crash", "malformed", "oversized"})
_PRE_SUBMIT = frozenset({"running", "proposal_ready"})
_BUSY = frozenset({"submitting", "outcome_unknown", "reconciling"})


def _same_typed(actual, expected):
    """Compare the small proposal schema without Python's bool/int coercion."""
    if type(expected) is dict:
        return (type(actual) is dict and actual.keys() == expected.keys()
                and all(_same_typed(actual[key], value) for key, value in expected.items()))
    if type(expected) is list:
        return (type(actual) is list and len(actual) == len(expected)
                and all(_same_typed(a, b) for a, b in zip(actual, expected)))
    if type(expected) is int:
        return type(actual) is int and actual == expected
    if type(expected) is float:
        return type(actual) in (int, float) and actual == expected and math.isfinite(actual)
    return type(actual) is type(expected) and actual == expected


@dataclass(frozen=True)
class CubeGoal:
    kind: str
    x: float
    name: str | None = None
    handle: EntityHandle | None = None

    def __post_init__(self):
        if type(self.x) not in (int, float) or not -8 <= self.x <= 8 or not math.isfinite(self.x):
            raise ProtocolError()
        object.__setattr__(self, "x", float(self.x))
        if self.kind == "create":
            if (type(self.name) is not str or re.fullmatch(r"[A-Za-z][A-Za-z0-9_]{0,31}", self.name) is None
                    or self.handle is not None):
                raise ProtocolError()
        elif self.kind == "move":
            if type(self.handle) is not EntityHandle or self.handle.world_id != "workshop" or self.name is not None:
                raise ProtocolError()
        else:
            raise ProtocolError()

    @classmethod
    def create(cls, name: str, x: float) -> CubeGoal:
        return cls("create", x, name=name)

    @classmethod
    def move(cls, handle: EntityHandle, x: float) -> CubeGoal:
        return cls("move", x, handle=handle)

    def _wire(self):
        return {"kind": self.kind, "x": self.x, "name": self.name,
                "handle": None if self.handle is None else self.handle.to_dict()}

    def _operations(self):
        transform = Transform(position_m=(self.x, 0, 0))
        if self.kind == "create":
            return (CreateCube(self.name), SetTransform(TemporaryTarget(self.name), transform))
        return (SetTransform(self.handle, transform),)


@dataclass(frozen=True)
class WorkerStatus:
    request_id: int
    generation: int
    state: str
    expected_revision: int
    late_results: int
    receipt: Receipt | None
    error_code: str | None
    worker_cleaned: bool

    def to_dict(self):
        return {"request_id": self.request_id, "generation": self.generation, "state": self.state,
                "expected_revision": self.expected_revision, "late_results": self.late_results,
                "receipt": None if self.receipt is None else self.receipt.to_dict(),
                "error_code": self.error_code, "worker_cleaned": self.worker_cleaned}


@dataclass
class _Request:
    request_id: int
    generation: int
    expected_revision: int
    goal: CubeGoal
    deadline: float
    mode: str
    delay_ms: int
    transaction_id: str | None
    state: str = "running"
    late_results: int = 0
    receipt: Receipt | None = None
    error_code: str | None = None
    worker_cleaned: bool = False
    proposal: tuple | None = None
    prepared: PreparedTransaction | None = field(default=None, repr=False)
    first_submission_started: bool = False


class WorkerSupervisor:
    """Take exclusive ownership of a fresh RetryPolicyClient until close().

    Status/cancellation acquire only a short local state lock. Pipe work,
    process reaping, negotiation and mutation I/O run outside it. There is no
    automatic submission, retry, reconciliation, or uncertain-key replacement.
    """
    def __init__(self, client: RetryPolicyClient):
        if not isinstance(client, RetryPolicyClient):
            raise ProtocolError()
        if client._pending is not None or client._uncertain:
            raise OutcomeUnknown()
        self._client = client
        self._lock = threading.Lock()
        self._stop = threading.Event()
        self._records: dict[int, _Request] = {}
        self._threads: set[threading.Thread] = set()
        self._generation = 1
        self._active: int | None = None
        self._blocked: int | None = None
        self._workers = 0
        self._closed = False

    def __repr__(self):
        return "WorkerSupervisor(local=True, authority=<parent-only>)"

    def _record(self, request_id):
        integer(request_id, 1, 8)
        try:
            return self._records[request_id]
        except KeyError:
            raise ProtocolError() from None

    @staticmethod
    def _view(record):
        return WorkerStatus(record.request_id, record.generation, record.state,
                            record.expected_revision, record.late_results, record.receipt,
                            record.error_code, record.worker_cleaned)

    def _terminal(self, record, state, code=None):
        record.state, record.error_code = state, code
        if self._active == record.request_id:
            self._active = None

    def _expire(self, record):
        if record.state in _PRE_SUBMIT and time.monotonic() >= record.deadline:
            self._terminal(record, "timed_out", "DEADLINE_EXPIRED")

    def status(self, request_id: int) -> WorkerStatus:
        with self._lock:
            record = self._record(request_id)
            self._expire(record)
            return self._view(record)

    def _spawn(self, target, *args):
        # Called with local lock held; starting a Python thread performs no
        # provider/network I/O. The child cannot enter our lock until released.
        thread = threading.Thread(target=self._thread_entry, args=(target, args),
                                  name="omniweft-lifecycle", daemon=False)
        self._threads.add(thread)
        try:
            thread.start()
        except Exception:
            self._threads.remove(thread)
            raise ProtocolError() from None

    def _thread_entry(self, target, args):
        try:
            target(*args)
        finally:
            with self._lock:
                self._threads.discard(threading.current_thread())

    def _start(self, goal, expected_revision, timeout_ms, mode, delay_ms, transaction_id, supersede):
        if type(goal) is not CubeGoal:
            raise ProtocolError()
        integer(expected_revision); integer(timeout_ms, 50, 5000); integer(delay_ms, 0, 2000)
        if type(mode) is not str or mode not in _MODES:
            raise ProtocolError()
        if transaction_id is not None:
            uuid(transaction_id)
        with self._lock:
            if self._closed:
                raise ProtocolError()
            if self._blocked is not None:
                raise OutcomeUnknown()
            if len(self._records) >= 8 or self._workers >= 2:
                raise ProtocolError()
            old = None if self._active is None else self._records[self._active]
            if old is not None:
                self._expire(old)
                if old.state in _PRE_SUBMIT and not supersede:
                    raise ProtocolError()
            if supersede:
                self._generation += 1
                if old is not None and old.state in _PRE_SUBMIT:
                    self._terminal(old, "superseded", "SUPERSEDED")
            number = len(self._records) + 1
            record = _Request(number, self._generation, expected_revision, goal,
                              time.monotonic() + timeout_ms / 1000, mode, delay_ms, transaction_id)
            self._records[number] = record
            self._active = number
            self._workers += 1
            try:
                self._spawn(self._worker, record)
            except ProtocolError:
                self._workers -= 1
                record.worker_cleaned = True
                self._terminal(record, "worker_failed", "WORKER_START_FAILED")
            return self._view(record)

    def start(self, goal, expected_revision, *, timeout_ms=1000, mode="normal", delay_ms=0, transaction_id=None):
        return self._start(goal, expected_revision, timeout_ms, mode, delay_ms, transaction_id, False)

    def supersede(self, goal, expected_revision, *, timeout_ms=1000, mode="normal", delay_ms=0, transaction_id=None):
        return self._start(goal, expected_revision, timeout_ms, mode, delay_ms, transaction_id, True)

    def cancel(self, request_id):
        with self._lock:
            record = self._record(request_id)
            self._expire(record)
            if record.state in _BUSY or record.prepared is not None:
                return "too_late"
            if record.state not in _PRE_SUBMIT:
                return "terminal"
            self._terminal(record, "cancelled", "CANCELLED")
            return "cancelled"

    def restart(self):
        with self._lock:
            if self._closed:
                raise ProtocolError()
            self._generation += 1
            if self._active is not None:
                record = self._records[self._active]
                self._expire(record)
                if record.state in _PRE_SUBMIT:
                    self._terminal(record, "superseded", "WORKER_RESTARTED")
            # Prepared authority survives provider generation changes.
            return self._generation

    def _accept(self, record, raw):
        value = fields(decode(raw, _FRAME_LIMIT),
                       {"schema_version", "request_id", "generation", "expected_revision", "operations"})
        integer(value["schema_version"], 1, 1)
        for name in ("request_id", "generation", "expected_revision"):
            integer(value[name])
            if value[name] != getattr(record, name):
                raise ProtocolError()
        operations = record.goal._operations()
        if not _same_typed(value["operations"], [operation.to_dict() for operation in operations]):
            raise ProtocolError()
        with self._lock:
            self._expire(record)
            if (self._closed or record.state != "running" or self._active != record.request_id
                    or record.generation != self._generation):
                record.late_results += 1
                return
            record.proposal = operations
            record.state = "proposal_ready"

    def _worker(self, record):
        process = None
        reader = None
        finished = threading.Event()
        received = []
        invalid = []
        try:
            with self._lock:
                if self._closed or time.monotonic() >= record.deadline:
                    self._expire(record)
                    return
            message = {"schema_version": 1, "request_id": record.request_id, "generation": record.generation,
                       "expected_revision": record.expected_revision, "goal": record.goal._wire(),
                       "mode": record.mode, "delay_ms": record.delay_ms}
            payload = json.dumps(message, allow_nan=False, separators=(",", ":")).encode("utf-8") + b"\n"
            if len(payload) > _FRAME_LIMIT:
                raise ProtocolError()
            environment = {"PYTHONNOUSERSITE": "1", "PYTHONDONTWRITEBYTECODE": "1", "PYTHONUTF8": "1"}
            if os.name == "nt" and "SystemRoot" in os.environ:
                environment["SystemRoot"] = os.environ["SystemRoot"]
            process = subprocess.Popen([sys.executable, "-B", "-s", str(Path(__file__).with_name("_proposal_worker.py"))],
                stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL,
                close_fds=True, env=environment,
                creationflags=subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0)

            def pipe_io():
                try:
                    process.stdin.write(payload)
                    process.stdin.flush()
                    process.stdin.close()
                    line = process.stdout.readline(_FRAME_LIMIT + 1)
                    if not line:
                        return
                    if len(line) > _FRAME_LIMIT or not line.endswith(b"\n"):
                        raise ProtocolError()
                    if process.stdout.read(1):
                        raise ProtocolError()
                    received.append(line)
                except Exception:
                    invalid.append(True)
                finally:
                    finished.set()

            reader = threading.Thread(target=pipe_io, name="omniweft-proposal-pipe", daemon=False)
            reader.start()
            while not finished.wait(.01):
                if self._stop.is_set() or time.monotonic() >= record.deadline:
                    break
            expired = time.monotonic() >= record.deadline
            if not finished.is_set() or invalid or self._stop.is_set():
                if process.poll() is None:
                    process.kill()
            code = process.wait(timeout=2)
            reader.join(timeout=2)
            if reader.is_alive():
                raise ProtocolError()
            with self._lock:
                self._expire(record)
            if received and code == 0:
                self._accept(record, received[0])
            elif not expired:
                with self._lock:
                    if record.state == "running":
                        state = "crashed" if code != 0 and not invalid else "worker_failed"
                        self._terminal(record, state, "WORKER_CRASHED" if state == "crashed" else "INVALID_PROPOSAL")
        except ProtocolError:
            with self._lock:
                if record.state == "running":
                    self._terminal(record, "worker_failed", "INVALID_PROPOSAL")
        except Exception:
            with self._lock:
                if record.state == "running":
                    self._terminal(record, "worker_failed", "WORKER_FAILED")
        finally:
            cleaned = True
            try:
                if process is not None:
                    if process.poll() is None:
                        process.kill()
                    process.wait(timeout=2)
                if reader is not None and reader.ident is not None:
                    reader.join(timeout=2)
                    cleaned = not reader.is_alive()
                if process is not None and cleaned:
                    for stream in (process.stdin, process.stdout):
                        if stream is not None:
                            stream.close()
            except Exception:
                cleaned = False
            with self._lock:
                record.worker_cleaned = cleaned
                if cleaned:
                    self._workers -= 1
                elif record.state in _PRE_SUBMIT:
                    self._terminal(record, "worker_failed", "CLEANUP_FAILED")

    def submit(self, request_id):
        with self._lock:
            record = self._record(request_id)
            self._expire(record)
            if record.state == "timed_out":
                return self._view(record)
            if self._closed or record.state != "proposal_ready" or self._active != request_id or record.generation != self._generation:
                raise ProtocolError()
            if self._blocked is not None:
                raise OutcomeUnknown()
            record.state = "submitting"
            self._blocked = request_id
            try:
                self._spawn(self._submit, record, False)
            except ProtocolError:
                self._blocked = None
                self._terminal(record, "worker_failed", "SUBMIT_START_FAILED")
            return self._view(record)

    def retry(self, request_id):
        with self._lock:
            record = self._record(request_id)
            if (self._closed or record.state != "outcome_unknown" or record.prepared is None
                    or not record.first_submission_started or self._blocked != request_id):
                raise ProtocolError()
            record.state, record.error_code = "submitting", None
            try:
                self._spawn(self._submit, record, True)
            except ProtocolError:
                record.state, record.error_code = "outcome_unknown", "OUTCOME_UNKNOWN"
            return self._view(record)

    def _submit(self, record, retry):
        try:
            if not retry:
                if time.monotonic() >= record.deadline:
                    with self._lock:
                        self._blocked = None
                        self._terminal(record, "timed_out", "DEADLINE_EXPIRED")
                    return
                prepared = self._client.prepare(record.proposal, record.expected_revision, transaction_id=record.transaction_id)
                with self._lock:
                    record.prepared = prepared
                with self._lock:
                    if time.monotonic() >= record.deadline:
                        record.state, record.error_code = "outcome_unknown", "DEADLINE_EXPIRED"
                        return
                    record.first_submission_started = True
            # The exact object is preserved even across provider restarts.
            receipt = self._client.submit(record.prepared)
            if type(receipt) is not Receipt:
                raise ProtocolError()
            with self._lock:
                record.receipt = receipt
                record.prepared = None
                self._blocked = None
                self._terminal(record, receipt.status)
        except ApiError as error:
            with self._lock:
                record.state = "outcome_unknown"
                record.error_code = error.code if error.code in RetryPolicyClient._error_codes else "OUTCOME_UNKNOWN"
        except Exception:
            with self._lock:
                record.state, record.error_code = "outcome_unknown", "OUTCOME_UNKNOWN"

    def reconcile(self, request_id):
        with self._lock:
            record = self._record(request_id)
            if self._closed or self._blocked != request_id or record.state != "outcome_unknown":
                raise ProtocolError()
            record.state, record.error_code = "reconciling", None
            try:
                self._spawn(self._reconcile, record)
            except ProtocolError:
                record.state, record.error_code = "outcome_unknown", "RECONCILIATION_FAILED"
            return self._view(record)

    def _reconcile(self, record):
        try:
            self._client.resync_policy()
            with self._lock:
                record.prepared = None
                self._blocked = None
                self._terminal(record, "reconciled")
        except Exception:
            with self._lock:
                record.state, record.error_code = "outcome_unknown", "RECONCILIATION_FAILED"

    def close(self):
        with self._lock:
            self._closed = True
            self._stop.set()
            for record in self._records.values():
                if record.state in _PRE_SUBMIT:
                    self._terminal(record, "cancelled", "SUPERVISOR_CLOSED")
        deadline = time.monotonic() + 12
        while True:
            with self._lock:
                threads = tuple(self._threads)
            if not threads:
                break
            for thread in threads:
                thread.join(max(0, deadline - time.monotonic()))
            if time.monotonic() >= deadline:
                raise ProtocolError()
        with self._lock:
            if self._workers:
                raise ProtocolError()
        # Unresolved prepared state remains queryable; closing is not reconciliation.

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_value, traceback):
        self.close()
