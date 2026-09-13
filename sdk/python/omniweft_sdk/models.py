# SPDX-License-Identifier: Apache-2.0
"""Strict immutable models for Omniweft's bounded local control profile."""
from __future__ import annotations

from dataclasses import asdict, dataclass, field
import json
import math
import re
from typing import Any

UINT64_MAX = (1 << 64) - 1
HEX64 = re.compile(r"[0-9a-f]{64}\Z")
UUID = re.compile(r"[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}\Z")


class ProtocolError(Exception):
    def __init__(self) -> None:
        super().__init__("CONTROL_PROTOCOL_ERROR")


class ApiError(Exception):
    def __init__(self, code: str, status: int) -> None:
        self.code, self.status = code, status
        super().__init__(code)


class OutcomeUnknown(Exception):
    def __init__(self) -> None:
        super().__init__("OUTCOME_UNKNOWN: observe and explicitly resync before another mutation")


def fields(value: Any, names: set[str]) -> dict:
    if not isinstance(value, dict) or set(value) != names:
        raise ProtocolError()
    return value


def integer(value: Any, minimum: int = 0, maximum: int = UINT64_MAX) -> int:
    if type(value) is not int or not minimum <= value <= maximum:
        raise ProtocolError()
    return value


def text(value: Any, maximum: int = 256) -> str:
    if not isinstance(value, str) or not value or len(value) > maximum:
        raise ProtocolError()
    return value


def uuid(value: Any) -> str:
    if not isinstance(value, str) or not UUID.fullmatch(value):
        raise ProtocolError()
    return value


def epoch(value: Any) -> str:
    if not isinstance(value, str) or not HEX64.fullmatch(value):
        raise ProtocolError()
    return value


def decode(data: bytes, limit: int = 4194304) -> Any:
    if len(data) > limit:
        raise ProtocolError()

    def pairs(items: list[tuple[str, Any]]) -> dict:
        out = {}
        for key, value in items:
            if key in out:
                raise ProtocolError()
            out[key] = value
        return out

    def invalid(_: str) -> None:
        raise ProtocolError()

    try:
        value = json.loads(data.decode("utf-8"), object_pairs_hook=pairs, parse_constant=invalid)
        pending = [(value, 0)]
        while pending:
            item, depth = pending.pop()
            if depth > 32:
                raise ProtocolError()
            if isinstance(item, float) and not math.isfinite(item):
                raise ProtocolError()
            if isinstance(item, dict):
                pending.extend((v, depth + 1) for v in item.values())
            elif isinstance(item, list):
                pending.extend((v, depth + 1) for v in item)
        return value
    except (UnicodeError, ValueError, RecursionError, OverflowError):
        raise ProtocolError() from None


def vector(value: Any, size: int) -> tuple[float, ...]:
    if not isinstance(value, (list, tuple)) or len(value) != size:
        raise ProtocolError()
    try:
        if any(type(x) not in (int, float) for x in value):
            raise ProtocolError()
        converted = tuple(float(x) for x in value)
        if any(not math.isfinite(x) for x in converted):
            raise ProtocolError()
        return converted
    except (OverflowError, ValueError):
        raise ProtocolError() from None


@dataclass(frozen=True)
class ConnectionInfo:
    host: str
    port: int
    token: str = field(repr=False)
    epoch: str = field(repr=False)
    session_ttl_ms: int

    def __post_init__(self) -> None:
        if self.host != "127.0.0.1":
            raise ProtocolError()
        integer(self.port, 1, 65535)
        epoch(self.token)
        epoch(self.epoch)
        if self.token == self.epoch:
            raise ProtocolError()
        integer(self.session_ttl_ms, 50, 300000)

    @classmethod
    def from_descriptor(cls, raw: bytes) -> ConnectionInfo:
        value = fields(decode(raw, 512), {
            "schema_version", "protocol_version", "host", "port", "token", "epoch", "session_ttl_ms"
        })
        if type(value["schema_version"]) is not int or value["schema_version"] != 1 or value["protocol_version"] != "0.1":
            raise ProtocolError()
        return cls(*(value[k] for k in ("host", "port", "token", "epoch", "session_ttl_ms")))


@dataclass(frozen=True)
class EntityHandle:
    world_id: str
    entity_uuid: str
    generation: int

    def __post_init__(self) -> None:
        text(self.world_id, 64)
        uuid(self.entity_uuid)
        integer(self.generation, 1)

    def to_dict(self) -> dict:
        return asdict(self)


@dataclass(frozen=True)
class Transform:
    position_m: tuple[float, ...] = (0.0, 0.0, 0.0)
    rotation_xyzw: tuple[float, ...] = (0.0, 0.0, 0.0, 1.0)
    scale: tuple[float, ...] = (1.0, 1.0, 1.0)

    def __post_init__(self) -> None:
        for name, size in (("position_m", 3), ("rotation_xyzw", 4), ("scale", 3)):
            object.__setattr__(self, name, vector(getattr(self, name), size))

    @classmethod
    def from_dict(cls, value: Any) -> Transform:
        return cls(**fields(value, {"position_m", "rotation_xyzw", "scale"}))

    def to_dict(self) -> dict:
        return {k: list(v) for k, v in asdict(self).items()}


@dataclass(frozen=True)
class Entity:
    prefab: str
    authoring_revision: int
    transform: Transform

    @classmethod
    def from_dict(cls, value: Any) -> Entity:
        v = fields(value, {"prefab", "authoring_revision", "transform"})
        return cls(text(v["prefab"]), integer(v["authoring_revision"]), Transform.from_dict(v["transform"]))


@dataclass(frozen=True)
class Slot:
    entity_uuid: str
    generation: int
    retired: bool
    entity: Entity | None

    @classmethod
    def from_dict(cls, value: Any) -> Slot:
        v = fields(value, {"entity_uuid", "generation", "retired", "entity"})
        if type(v["retired"]) is not bool:
            raise ProtocolError()
        e = None if v["entity"] is None else Entity.from_dict(v["entity"])
        if v["retired"] and e is not None:
            raise ProtocolError()
        return cls(uuid(v["entity_uuid"]), integer(v["generation"], 1), v["retired"], e)


@dataclass(frozen=True)
class Snapshot:
    format_version: int
    world_id: str
    seed: int
    max_slots: int
    world_revision: int
    slots: tuple[Slot, ...]

    @classmethod
    def from_dict(cls, value: Any) -> Snapshot:
        v = fields(value, {"format_version", "world_id", "seed", "max_slots", "world_revision", "slots"})
        integer(v["format_version"], 1, 1)
        count = integer(v["max_slots"], 1, 1024)
        if not isinstance(v["slots"], list) or len(v["slots"]) > count:
            raise ProtocolError()
        slots = tuple(Slot.from_dict(s) for s in v["slots"])
        if len({s.entity_uuid for s in slots}) != len(slots):
            raise ProtocolError()
        revision = integer(v["world_revision"])
        if any(s.entity and s.entity.authoring_revision > revision for s in slots):
            raise ProtocolError()
        return cls(1, text(v["world_id"], 64), integer(v["seed"], 0, (1 << 32) - 1), count, revision, slots)

    def get_entity(self, handle: EntityHandle) -> Entity:
        if handle.world_id != self.world_id:
            raise ApiError("NOT_FOUND", 404)
        for slot in self.slots:
            if slot.entity_uuid == handle.entity_uuid:
                if slot.generation != handle.generation or slot.entity is None or slot.retired:
                    raise ApiError("STALE_HANDLE", 409)
                return slot.entity
        raise ApiError("NOT_FOUND", 404)

    def to_dict(self) -> dict:
        value = asdict(self)
        value["slots"] = list(value["slots"])
        for slot in value["slots"]:
            if slot["entity"]:
                slot["entity"]["transform"] = {k: list(v) for k, v in slot["entity"]["transform"].items()}
        return value


@dataclass(frozen=True)
class CreatedBinding:
    temporary_id: str
    world_id: str
    entity_uuid: str
    generation: int

    def handle(self) -> EntityHandle:
        return EntityHandle(self.world_id, self.entity_uuid, self.generation)


@dataclass(frozen=True)
class ReceiptError:
    code: str
    path: str
    message: str = field(repr=False)
    operation_index: int | None = None


@dataclass(frozen=True)
class Receipt:
    status: str
    durability: str
    transaction_id: str
    world_revision: int
    created: tuple[CreatedBinding, ...]
    errors: tuple[ReceiptError, ...]

    @classmethod
    def from_dict(cls, value: Any) -> Receipt:
        v = fields(value, {"status", "durability", "transaction_id", "world_revision", "created", "errors"})
        if v["status"] not in ("committed", "rejected") or v["durability"] != "volatile":
            raise ProtocolError()
        if any(not isinstance(v[k], list) or len(v[k]) > 256 for k in ("created", "errors")):
            raise ProtocolError()
        created = []
        for item in v["created"]:
            x = fields(item, {"temporary_id", "world_id", "entity_uuid", "generation"})
            created.append(CreatedBinding(text(x["temporary_id"], 64), text(x["world_id"], 64),
                                          uuid(x["entity_uuid"]), integer(x["generation"], 1)))
        errors = []
        for item in v["errors"]:
            if not isinstance(item, dict):
                raise ProtocolError()
            x = fields(item, {"code", "path", "message"} | ({"operation_index"} if "operation_index" in item else set()))
            if (not isinstance(x["code"], str) or re.fullmatch(r"[A-Z][A-Z0-9_]{0,63}", x["code"]) is None
                    or not isinstance(x["path"], str) or len(x["path"]) > 256
                    or re.fullmatch(r"(?:/[a-z_][a-z_0-9]*|/[0-9]+)*", x["path"]) is None):
                raise ProtocolError()
            index = integer(x["operation_index"], 0, 255) if "operation_index" in x else None
            errors.append(ReceiptError(text(x["code"], 64), x["path"], text(x["message"], 1024), index))
        if v["status"] == "committed" and errors or v["status"] == "rejected" and (created or not errors):
            raise ProtocolError()
        return cls(v["status"], "volatile", uuid(v["transaction_id"]), integer(v["world_revision"]),
                   tuple(created), tuple(errors))

    def to_dict(self) -> dict:
        v = asdict(self)
        v["created"] = list(v["created"])
        v["errors"] = [{k: x for k, x in e.items() if x is not None} for e in v["errors"]]
        return v


@dataclass(frozen=True)
class CreateCube:
    temporary_id: str

    def to_dict(self) -> dict:
        return {"type": "entity.create", "temporary_id": text(self.temporary_id, 64), "prefab": "builtin.unit_cube"}


@dataclass(frozen=True)
class SetTransform:
    target: EntityHandle
    transform: Transform

    def to_dict(self) -> dict:
        return {"type": "transform.set", "target": self.target.to_dict(), **self.transform.to_dict()}


@dataclass(frozen=True)
class DeleteEntity:
    target: EntityHandle

    def to_dict(self) -> dict:
        return {"type": "entity.delete", "target": self.target.to_dict(), "child_policy": "reject_if_children"}
