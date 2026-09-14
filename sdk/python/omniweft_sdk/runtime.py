# SPDX-License-Identifier: Apache-2.0
"""The opt-in version 1 fixed-step runtime observation profile."""
from __future__ import annotations

from dataclasses import asdict, dataclass
from .models import ProtocolError, Snapshot, fields, integer


@dataclass(frozen=True)
class PresentationStatus:
    enabled: bool
    ready: bool
    frame_count: int
    world_revision: int
    snapshot_sequence: int

    @classmethod
    def from_dict(cls, value: object) -> PresentationStatus:
        v = fields(value, {"enabled", "ready", "frame_count", "world_revision", "snapshot_sequence"})
        if type(v["enabled"]) is not bool or type(v["ready"]) is not bool:
            raise ProtocolError()
        for key in ("frame_count", "world_revision", "snapshot_sequence"):
            integer(v[key])
        if (not v["enabled"] and (v["ready"] or any(v[k] for k in
                ("frame_count", "world_revision", "snapshot_sequence")))):
            raise ProtocolError()
        if v["frame_count"] == 0 and v["world_revision"] != 0:
            raise ProtocolError()
        if not v["ready"] and v["frame_count"]:
            raise ProtocolError()
        if (v["frame_count"] == 0) != (v["snapshot_sequence"] == 0):
            raise ProtocolError()
        return cls(**v)


@dataclass(frozen=True)
class RuntimeStatus:
    schema_version: int
    tick_rate_hz: int
    max_catch_up_steps: int
    simulation_tick: int
    snapshot_sequence: int
    overload_count: int
    dropped_ticks: int
    remainder_units: int
    snapshot: Snapshot
    presentation: PresentationStatus

    @classmethod
    def from_dict(cls, value: object) -> RuntimeStatus:
        v = fields(value, {"schema_version", "tick_rate_hz", "max_catch_up_steps",
                          "simulation_tick", "snapshot_sequence", "overload_count",
                          "dropped_ticks", "remainder_units", "snapshot", "presentation"})
        for key, expected in (("schema_version", 1), ("tick_rate_hz", 60),
                              ("max_catch_up_steps", 4)):
            if integer(v[key]) != expected:
                raise ProtocolError()
        for key in ("simulation_tick", "snapshot_sequence", "overload_count", "dropped_ticks"):
            integer(v[key])
        integer(v["remainder_units"], 0, 999999999)
        if (v["snapshot_sequence"] != v["simulation_tick"] + 1
                or v["overload_count"] > v["dropped_ticks"]):
            raise ProtocolError()
        snapshot = Snapshot.from_dict(v["snapshot"])
        presentation = PresentationStatus.from_dict(v["presentation"])
        if (presentation.world_revision > snapshot.world_revision
                or presentation.snapshot_sequence > v["snapshot_sequence"]):
            raise ProtocolError()
        return cls(**{**v, "snapshot": snapshot, "presentation": presentation})

    def to_dict(self) -> dict:
        return {**asdict(self), "snapshot": self.snapshot.to_dict()}
