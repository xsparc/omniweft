# SPDX-License-Identifier: Apache-2.0
"""Direct loopback client; mutations are never automatically retried."""
from __future__ import annotations

from dataclasses import dataclass, field
import http.client
import json
import socket
import threading
import time
from typing import Sequence
import uuid as uuid_module

from .models import (ApiError, ConnectionInfo, CreateCube, DeleteEntity, EntityHandle,
                     OutcomeUnknown, ProtocolError, Receipt, SetTransform, Snapshot,
                     Transform, UINT64_MAX, decode, epoch, fields, integer, text)

from .runtime import RuntimeStatus

MAX_RESPONSE = 4194304
ERROR_CODES = frozenset({
    "NOT_AUTHORIZED", "SESSION_EXPIRED", "INVALID_SCHEMA", "UNSUPPORTED_VERSION",
    "BUDGET_EXCEEDED", "REQUIRES_RESYNC", "NOT_FOUND", "UNSUPPORTED_OPERATION",
    "NONFINITE_VALUE", "DEADLINE_EXPIRED", "QUEUE_FULL"
})


@dataclass(frozen=True)
class Limits:
    max_header_bytes: int
    max_body_bytes: int
    max_response_bytes: int
    max_operations: int
    max_slots: int
    request_timeout_ms: int
    session_ttl_ms: int

    @classmethod
    def from_dict(cls, value: object) -> Limits:
        names = set(cls.__dataclass_fields__)
        v = fields(value, names)
        for key, expected in {
            "max_header_bytes": 16384, "max_body_bytes": 1048576,
            "max_response_bytes": MAX_RESPONSE, "max_operations": 256, "request_timeout_ms": 1000
        }.items():
            integer(v[key], expected, expected)
        integer(v["max_slots"], 1, 1024)
        integer(v["session_ttl_ms"], 50, 300000)
        return cls(**v)


@dataclass(frozen=True)
class Capabilities:
    protocol_version: str
    epoch: str = field(repr=False)
    next_sequence: int
    world_id: str
    world_revision: int
    operations: tuple[str, ...]
    limits: Limits
    admission: str
    durability: str
    retry_mode: str

    @classmethod
    def from_dict(cls, value: object) -> Capabilities:
        v = fields(value, set(cls.__dataclass_fields__))
        if (v["protocol_version"] != "0.1" or v["admission"] != "synchronous"
                or v["durability"] != "volatile" or v["retry_mode"] != "resync_only"
                or v["operations"] != ["entity.create", "transform.set", "entity.delete"]):
            raise ProtocolError()
        return cls("0.1", epoch(v["epoch"]), integer(v["next_sequence"], 1),
                   text(v["world_id"], 64), integer(v["world_revision"]), tuple(v["operations"]),
                   Limits.from_dict(v["limits"]), "synchronous", "volatile", "resync_only")


class _HeaderBudget:
    def __init__(self, source: object) -> None:
        self.source, self.used = source, 0

    def readline(self, limit: int = -1) -> bytes:
        remaining = 16384 - self.used
        value = self.source.readline(min(limit, remaining + 1) if limit >= 0 else remaining + 1)
        self.used += len(value)
        if self.used > 16384:
            raise ProtocolError()
        return value

    def __getattr__(self, name: str) -> object:
        return getattr(self.source, name)


class _BoundedResponse(http.client.HTTPResponse):
    def __init__(self, *args: object, **kwargs: object) -> None:
        super().__init__(*args, **kwargs)
        self.fp = _HeaderBudget(self.fp)


class Client:
    _error_codes = ERROR_CODES

    def __init__(self, info: ConnectionInfo, *, timeout_ms: int = 3000) -> None:
        if not isinstance(info, ConnectionInfo):
            raise ProtocolError()
        integer(timeout_ms, 100, 10000)
        self._info, self._timeout = info, timeout_ms / 1000
        self._lock = threading.RLock()
        self._caps: Capabilities | None = None
        self._next: int | None = None
        self._uncertain = False

    def __repr__(self) -> str:
        return "Client(local=True, credentials=<private>)"

    def _request(self, method: str, route: str, body: dict | None = None, *, mutating: bool = False) -> dict:
        try:
            payload = None if body is None else json.dumps(
                body, ensure_ascii=False, allow_nan=False, separators=(",", ":")).encode("utf-8")
        except (ValueError, TypeError, OverflowError, UnicodeError):
            raise ProtocolError() from None
        if payload is not None and len(payload) > 1048576:
            raise ProtocolError()
        connection = http.client.HTTPConnection("127.0.0.1", self._info.port, timeout=self._timeout)
        connection.response_class = _BoundedResponse
        active_socket: list[socket.socket] = []
        expired = threading.Event()
        end = time.monotonic() + self._timeout

        def abort() -> None:
            expired.set()
            for candidate in active_socket:
                try:
                    candidate.shutdown(socket.SHUT_RDWR)
                except OSError:
                    pass

        timer = threading.Timer(self._timeout, abort)
        timer.daemon = True
        timer.start()
        try:
            headers = {
                "Authorization": "Bearer " + self._info.token,
                "X-Omniweft-Protocol": "0.1",
                "Host": "127.0.0.1:" + str(self._info.port),
                "Connection": "close"
            }
            if payload is not None:
                headers["Content-Type"] = "application/json"
            connection.connect()
            connection.auto_open = 0
            if connection.sock is None:
                raise ProtocolError()
            active_socket.append(connection.sock)
            remaining = end - time.monotonic()
            if expired.is_set() or remaining <= 0:
                raise ProtocolError()
            connection.sock.settimeout(remaining)
            connection.request(method, route, payload, headers)
            if expired.is_set() or time.monotonic() >= end:
                raise ProtocolError()
            response = connection.getresponse()
            response_headers: dict[str, str] = {}
            for key, value in response.getheaders():
                name = key.lower()
                if name in response_headers:
                    raise ProtocolError()
                response_headers[name] = value
            length = response_headers.get("content-length", "")
            if (len(response_headers) > 64 or not length.isascii() or not length.isdecimal()
                    or len(length) > 10 or int(length) > MAX_RESPONSE
                    or response_headers.get("content-type") != "application/json"
                    or response_headers.get("connection", "").lower() != "close"
                    or "transfer-encoding" in response_headers):
                raise ProtocolError()
            data = response.read(int(length) + 1)
            if len(data) != int(length) or expired.is_set() or time.monotonic() >= end:
                raise ProtocolError()
            value = decode(data)
            pending = [(value, True)]
            while pending:
                item, root = pending.pop()
                if isinstance(item, dict):
                    for key, child in item.items():
                        if self._info.token in key or self._info.epoch in key:
                            raise ProtocolError()
                        if root and key == "epoch" and child == self._info.epoch:
                            continue
                        pending.append((child, False))
                elif isinstance(item, list):
                    pending.extend((child, False) for child in item)
                elif isinstance(item, str) and (self._info.token in item or self._info.epoch in item):
                    raise ProtocolError()
            if response.status != 200:
                error = fields(value, {"protocol_version", "status", "error"})
                item = fields(error["error"], {"code", "path"})
                if (error["protocol_version"] != "0.1" or error["status"] != "rejected"
                        or item["code"] not in self._error_codes or not isinstance(item["path"], str)
                        or len(item["path"]) > 256 or not 400 <= response.status < 500):
                    raise ProtocolError()
                if item["code"] == "REQUIRES_RESYNC":
                    self._uncertain = True
                raise ApiError(item["code"], response.status)
            if not isinstance(value, dict):
                raise ProtocolError()
            return value
        except ApiError:
            raise
        except (OSError, http.client.HTTPException, ProtocolError, ValueError, TypeError, OverflowError):
            if mutating:
                self._uncertain = True
                raise OutcomeUnknown() from None
            raise ProtocolError() from None
        finally:
            timer.cancel()
            timer.join()
            connection.close()

    def capabilities(self) -> Capabilities:
        with self._lock:
            caps = Capabilities.from_dict(self._request("GET", "/v0/capabilities"))
            if caps.epoch != self._info.epoch or caps.limits.session_ttl_ms != self._info.session_ttl_ms:
                raise ProtocolError()
            if self._next is not None and caps.next_sequence < self._next:
                raise ProtocolError()
            self._caps, self._next = caps, caps.next_sequence
            return caps

    def _ensure_negotiated(self) -> Capabilities:
        return self._caps if self._caps is not None else self.capabilities()

    def _wrapper(self, value: object, field_name: str) -> dict:
        v = fields(value, {"protocol_version", "epoch", "next_sequence", field_name})
        if v["protocol_version"] != "0.1" or epoch(v["epoch"]) != self._info.epoch:
            raise ProtocolError()
        integer(v["next_sequence"], 1)
        if self._next is not None and v["next_sequence"] < self._next:
            raise ProtocolError()
        return v

    def observe(self) -> Snapshot:
        with self._lock:
            caps = self._ensure_negotiated()
            v = self._wrapper(self._request("POST", "/v0/observe",
                              {"protocol_version": "0.1", "world_id": caps.world_id}), "snapshot")
            snapshot = Snapshot.from_dict(v["snapshot"])
            if snapshot.world_id != caps.world_id or snapshot.max_slots != caps.limits.max_slots:
                raise ProtocolError()
            self._next = v["next_sequence"]
            return snapshot

    def runtime(self) -> RuntimeStatus:
        """Read opt-in runtime profile 1; legacy hosts reject this route."""
        with self._lock:
            caps = self._ensure_negotiated()
            v = self._wrapper(self._request("GET", "/v0/runtime"), "runtime")
            status = RuntimeStatus.from_dict(v["runtime"])
            if (status.snapshot.world_id != caps.world_id
                    or status.snapshot.max_slots != caps.limits.max_slots):
                raise ProtocolError()
            self._next = v["next_sequence"]
            return status

    def get_entity(self, handle: EntityHandle):
        return self.observe().get_entity(handle)

    def resync(self) -> Snapshot:
        with self._lock:
            self.capabilities()
            snapshot = self.observe()
            self._uncertain = False
            return snapshot

    def transact(self, operations: Sequence[CreateCube | SetTransform | DeleteEntity],
                 expected_revision: int, *, transaction_id: str | None = None) -> Receipt:
        with self._lock:
            if self._uncertain:
                raise OutcomeUnknown()
            caps = self._ensure_negotiated()
            integer(expected_revision)
            if not 1 <= len(operations) <= caps.limits.max_operations:
                raise ProtocolError()
            if any(type(op) not in (CreateCube, SetTransform, DeleteEntity) for op in operations):
                raise ProtocolError()
            if self._next is None or self._next >= UINT64_MAX:
                self._uncertain = True
                raise OutcomeUnknown()
            from .models import uuid
            identifier = uuid(transaction_id) if transaction_id is not None else str(uuid_module.uuid4())
            sequence = self._next
            envelope = {
                "protocol_version": "0.1", "world_id": caps.world_id, "transaction_id": identifier,
                "idempotency": {"epoch": self._info.epoch, "sequence": sequence},
                "expected_world_revision": expected_revision,
                "apply_at": {"mode": "next_tick", "expires_after_ticks": 120},
                "budget": {"max_operations": len(operations), "max_blob_bytes": 0},
                "operations": [op.to_dict() for op in operations]
            }
            value = self._request("POST", "/v0/transactions", envelope, mutating=True)
            try:
                v = self._wrapper(value, "receipt")
                receipt = Receipt.from_dict(v["receipt"])
                if (v["next_sequence"] != sequence + 1 or receipt.transaction_id != identifier
                        or any(binding.world_id != caps.world_id for binding in receipt.created)):
                    raise ProtocolError()
                self._next = v["next_sequence"]
                return receipt
            except ProtocolError:
                self._uncertain = True
                raise OutcomeUnknown() from None

    def create_cube(self, temporary_id: str, expected_revision: int) -> Receipt:
        return self.transact([CreateCube(temporary_id)], expected_revision)

    def move(self, handle: EntityHandle, transform: Transform, expected_revision: int) -> Receipt:
        return self.transact([SetTransform(handle, transform)], expected_revision)

    def delete(self, handle: EntityHandle, expected_revision: int) -> Receipt:
        return self.transact([DeleteEntity(handle)], expected_revision)
