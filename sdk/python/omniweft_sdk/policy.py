# SPDX-License-Identifier: Apache-2.0
"""Opt-in immutable policy profile and private two-principal fixture session."""
from __future__ import annotations
from dataclasses import dataclass, field
import json
import queue
import threading

from .client import Client
from .models import ConnectionInfo, ProtocolError, decode, epoch, fields, integer
from .session import NativeSession


@dataclass(frozen=True)
class PolicyLimits:
    max_operations: int
    max_body_bytes: int
    retained_bytes: int
    working_bytes: int
    observation_bytes: int
    requests: int
    global_requests: int


@dataclass(frozen=True)
class PolicyUsage:
    retained_bytes: int
    working_bytes: int
    requests: int
    global_requests: int


@dataclass(frozen=True)
class PolicyStatus:
    principal: str
    world_revision: int
    next_sequence: int
    limits: PolicyLimits
    usage: PolicyUsage
    lower: tuple[float, float, float]
    upper: tuple[float, float, float]
    epoch: str = field(repr=False)
    read_scope: str = "whole_world"
    memory_model: str = "charged-resource-bytes-v1"

    @classmethod
    def from_dict(cls, value: object, principal: str) -> PolicyStatus:
        v = fields(value, {"protocol_version", "epoch", "next_sequence", "world_id", "world_revision", "policy"})
        p = fields(v["policy"], {"schema_version", "principal", "read_scope", "write_bounds_m",
                                 "memory_model", "limits", "usage"})
        if (v["protocol_version"] != "0.1" or v["world_id"] != "workshop"
                or principal not in ("west", "east") or p["principal"] != principal
                or p["read_scope"] != "whole_world" or p["memory_model"] != "charged-resource-bytes-v1"):
            raise ProtocolError()
        integer(p["schema_version"], 1, 1)
        expected = (4, 16384, 512, 98304, 512, 1, 2) if principal == "west" else (4, 16384, 2048, 262144, 4096, 1, 2)
        limits = fields(p["limits"], set(PolicyLimits.__dataclass_fields__))
        for name, number in zip(PolicyLimits.__dataclass_fields__, expected):
            integer(limits[name], number, number)
        bounds = fields(p["write_bounds_m"], {"lower", "upper"})
        lower, upper = ((-8, -4, -4), (-1, 4, 4)) if principal == "west" else ((1, -4, -4), (8, 4, 4))
        for name, expected_bound in (("lower", lower), ("upper", upper)):
            items = bounds[name]
            if (type(items) is not list or len(items) != 3
                    or any(type(x) not in (int, float) or x != y for x, y in zip(items, expected_bound))):
                raise ProtocolError()
        usage = fields(p["usage"], set(PolicyUsage.__dataclass_fields__))
        for name in PolicyUsage.__dataclass_fields__:
            integer(usage[name], 0, limits[name])
        if (usage["global_requests"] < usage["requests"]
                or (usage["requests"] == 0 and usage["working_bytes"] != 0)
                or (usage["requests"] == 1 and usage["working_bytes"] < 73728)):
            raise ProtocolError()
        return cls(principal, integer(v["world_revision"]), integer(v["next_sequence"], 1),
                   PolicyLimits(**limits), PolicyUsage(**usage), lower, upper, epoch(v["epoch"]))


class PolicyClient(Client):
    def __init__(self, info: ConnectionInfo, principal: str) -> None:
        super().__init__(info)
        if principal not in ("west", "east"):
            raise ProtocolError()
        self._principal = principal

    def policy_status(self) -> PolicyStatus:
        """Small exempt control query; its observation grant covers the whole world."""
        with self._lock:
            value = PolicyStatus.from_dict(self._request("GET", "/v0/policy"), self._principal)
            if value.epoch != self._info.epoch or (self._next is not None and value.next_sequence < self._next):
                raise ProtocolError()
            self._next = value.next_sequence
            return value

    def resync_policy(self) -> PolicyStatus:
        """Recover authoring revision/sequence when a full observation exceeds quota."""
        with self._lock:
            self.capabilities()
            value = self.policy_status()
            self._uncertain = False
            return value


class PolicySession(NativeSession):
    def __init__(self, executable, *, session_ttl_ms=30000, max_runtime_ms=60000, max_requests=1024):
        super().__init__(executable, session_ttl_ms=session_ttl_ms, max_slots=8,
                         max_runtime_ms=max_runtime_ms, max_requests=max_requests)
        self._principals: dict[str, ConnectionInfo] = {}

    def __repr__(self) -> str:
        return "PolicySession(local=True, principals=2, credentials=<private>)"

    def _descriptor(self) -> ConnectionInfo:
        if self._process is None or self._process.stdout is None:
            raise ProtocolError()
        stream = self._process.stdout
        result: queue.Queue[bytes | None] = queue.Queue(maxsize=1)

        def read():
            try:
                result.put_nowait(stream.readline(2049))
            except (OSError, ValueError):
                result.put_nowait(None)

        reader = threading.Thread(target=read, name="omniweft-private-policy", daemon=True)
        reader.start()
        try:
            data = result.get(timeout=3)
            if data is None or not data.endswith(b"\n") or len(data) > 2048:
                raise ProtocolError()
            wrapper = fields(decode(data[:-1]), {"schema_version", "profile", "principals"})
            integer(wrapper["schema_version"], 2, 2)
            if wrapper["profile"] != "policy.v1":
                raise ProtocolError()
            descriptors = fields(wrapper["principals"], {"west", "east"})
            parsed = {}
            for name in ("west", "east"):
                info = ConnectionInfo.from_descriptor(json.dumps(descriptors[name], separators=(",", ":")).encode("utf-8"))
                if info.session_ttl_ms != self._config[0]:
                    raise ProtocolError()
                old = self._principals.get(name)
                if old is not None and (info.token == old.token or info.epoch == old.epoch or info.port != old.port):
                    raise ProtocolError()
                parsed[name] = info
            west, east = parsed["west"], parsed["east"]
            if west.port != east.port or west.token == east.token or west.epoch == east.epoch:
                raise ProtocolError()
            self._principals = parsed
            return west
        except (queue.Empty, ProtocolError, ValueError, TypeError, UnicodeError):
            self._terminate()
            raise ProtocolError() from None
        finally:
            reader.join(timeout=1)

    def client(self, principal: str = "west") -> PolicyClient:
        with self._lock:
            if (self._process is None or self._process.poll() is not None
                    or principal not in self._principals):
                raise ProtocolError()
            return PolicyClient(self._principals[principal], principal)

    def renew(self) -> PolicyClient:
        super().renew()
        return self.client()

    def _terminate(self) -> None:
        self._principals.clear()
        super()._terminate()
