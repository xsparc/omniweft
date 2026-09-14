# SPDX-License-Identifier: Apache-2.0
"""Independent preregistered agent fixture and authoring-byte expectations."""
import copy
import hashlib
import json
import math
from pathlib import Path
import struct

from sdk_test_support import Evidence as SdkEvidence, digest, main_guard

from render_test_support import CHECKS, Failure, exact, require

ROOT = Path(__file__).resolve().parents[1]
FIXTURES = ROOT / "tests/fixtures/agents"


def strict_json(data):
    def pairs(items):
        result = {}
        for key, value in items:
            if key in result:
                raise Failure("agent JSON duplicate field")
            result[key] = value
        return result
    def number(value):
        result = float(value)
        if not math.isfinite(result):
            raise Failure("agent JSON nonfinite number")
        return result
    def constant(unused):
        raise Failure("agent JSON nonfinite token")
    try:
        return json.loads(data.decode("utf-8"), object_pairs_hook=pairs,
                          parse_float=number, parse_constant=constant)
    except (ValueError, UnicodeError):
        raise Failure("agent JSON syntax or encoding") from None


FIXTURE = strict_json((FIXTURES / "mock-builder.json").read_bytes())


def canonical(state):
    """Encode the published OWOBJ001 byte layout without any native/SDK import."""
    def text(value):
        raw = value.encode("ascii")
        return struct.pack("<I", len(raw)) + raw
    data = bytearray(b"OWOBJ001") + text(state["world_id"])
    data += struct.pack("<IIQI", state["seed"], state["max_slots"],
                        state["world_revision"], len(state["slots"]))
    for slot in state["slots"]:
        data += slot["entity_uuid"].encode("ascii")
        data += struct.pack("<QBB", slot["generation"], slot["retired"], slot["entity"] is not None)
        entity = slot["entity"]
        if entity is not None:
            data += text(entity["prefab"]) + struct.pack("<Q", entity["authoring_revision"])
            t = entity["transform"]
            values = t["position_m"] + t["rotation_xyzw"] + t["scale"]
            data += struct.pack("<10d", *(0.0 if v == 0 else v for v in values))
    return bytes(data)


def snapshot(revision, fixture=FIXTURE):
    if type(revision) is not int or not 0 <= revision <= 3:
        raise Failure("known agent fixture revision")
    return {"format_version": 1, "world_id": fixture["world_id"],
            "seed": fixture["seed"], "max_slots": fixture["max_slots"],
            "world_revision": revision, "slots": [
                {"entity_uuid": obj["entity_uuid"], "generation": 1, "retired": False,
                 "entity": {"prefab": "builtin.unit_cube",
                            "authoring_revision": obj["authoring_revision"],
                            "transform": copy.deepcopy(obj["transform"])}}
                for obj in fixture["objects"][:revision]]}


def receipt(revision):
    obj = FIXTURE["objects"][revision - 1]
    return {"status": "committed", "durability": "volatile",
            "transaction_id": FIXTURE["transaction_ids"][revision - 1],
            "world_revision": revision, "created": [{"temporary_id": obj["temporary_id"],
                "world_id": FIXTURE["world_id"], "entity_uuid": obj["entity_uuid"], "generation": 1}],
            "errors": []}


def compare(actual, expected, label):
    """Identity/counter/flag types stay exact; only authored numbers permit int/float."""
    if type(expected) is dict:
        require(type(actual) is dict and actual.keys() == expected.keys(), label + ": exact fields")
        for key in expected:
            compare(actual[key], expected[key], label + "/" + key)
    elif type(expected) is list:
        require(type(actual) is list and len(actual) == len(expected), label + ": exact length")
        for i, item in enumerate(expected):
            compare(actual[i], item, label + "/" + str(i))
    elif type(expected) is bool:
        require(type(actual) is bool and actual == expected, label + ": boolean")
    elif type(expected) is int:
        require(type(actual) is int and actual == expected, label + ": integer")
    elif type(expected) is float:
        require(type(actual) in (int, float) and math.isfinite(actual) and actual == expected,
                label + ": authored number")
    else:
        require(type(actual) is type(expected) and actual == expected, label + ": value")


def expected_state(revision, fixture=FIXTURE):
    value = snapshot(revision, fixture)
    # JSON authoring vectors are numeric; they are not identity/counter fields.
    for slot in value["slots"]:
        for key, values in slot["entity"]["transform"].items():
            slot["entity"]["transform"][key] = [float(v) for v in values]
    return value


def validate_fixture(fixture=FIXTURE):
    require(fixture["schema_version"] == 1 and fixture["world_id"] == "workshop"
            and fixture["seed"] == 7 and fixture["max_slots"] == 8, "frozen agent fixture configuration")
    for revision, record in enumerate(fixture["canonical_states"]):
        state = expected_state(revision, fixture)
        raw = canonical(state)
        require(record["world_revision"] == revision and record["bytes"] == len(raw),
                "preregistered canonical revision and length")
        require(record["sha256"] == hashlib.sha256(raw).hexdigest(),
                "preregistered canonical fixture hash")


def inspect_state(actual, revision, label="agent state"):
    compare(actual, expected_state(revision), label)
    raw = canonical(actual)
    require(hashlib.sha256(raw).hexdigest() == FIXTURE["canonical_states"][revision]["sha256"],
            label + ": frozen authoring hash")
    return raw


def inspect_authoring(actual, canonical_hex, canonical_sha256, revision):
    expected_bytes = inspect_state(actual, revision)
    require(type(canonical_hex) is str and canonical_hex == expected_bytes.hex(),
            "agent native canonical bytes")
    require(type(canonical_sha256) is str and canonical_sha256 ==
            FIXTURE["canonical_states"][revision]["sha256"], "agent reported authoring hash")


class Evidence(SdkEvidence):
    """Reuse only generic provenance/privacy capture; expected values remain local."""
    def __init__(self, directory, executable, mode="cpu", proof="independent_oracle"):
        require(mode in ("cpu", "gpu"), "known agent evidence lane")
        self.mode = mode
        super().__init__(directory, executable, proof)
        self.manifest.update(work_item="PR-006", example="agents.mock_builder", mode=mode,
                             lanes={"cpu": "running", "gpu": "running" if mode == "gpu" else "not_run"})
        if directory and mode == "gpu":
            require(self.manifest["environment"]["build"]["vulkan_enabled"],
                    "agent GPU evidence uses a Vulkan-enabled baseline")

    def finish(self, status, failure=None):
        self.manifest["lanes"]["gpu"] = status if self.mode == "gpu" else "not_run"
        super().finish(status, failure)


def uint(value, label, maximum=(1 << 64) - 1):
    require(type(value) is int and 0 <= value <= maximum, label + ": unsigned integer")
    return value


def inspect_runtime(value, revision, gpu, previous=None):
    keys = {"schema_version", "tick_rate_hz", "max_catch_up_steps", "simulation_tick",
            "snapshot_sequence", "overload_count", "dropped_ticks", "remainder_units",
            "snapshot", "presentation"}
    require(type(value) is dict and value.keys() == keys, "runtime exact public fields")
    for key, expected in (("schema_version", 1), ("tick_rate_hz", 60), ("max_catch_up_steps", 4)):
        require(type(value[key]) is int and value[key] == expected, "runtime fixed " + key)
    for key in ("simulation_tick", "snapshot_sequence", "overload_count", "dropped_ticks"):
        uint(value[key], "runtime " + key)
    uint(value["remainder_units"], "runtime fractional phase", 999999999)
    require(value["snapshot_sequence"] == value["simulation_tick"] + 1,
            "each executed tick has one complete publication")
    require(value["overload_count"] <= value["dropped_ticks"], "overload events have actual whole-step debt")
    inspect_state(value["snapshot"], revision, "runtime authoring snapshot")
    presentation = value["presentation"]
    require(type(presentation) is dict and presentation.keys() ==
            {"enabled", "ready", "frame_count", "world_revision", "snapshot_sequence"},
            "presentation exact public fields")
    require(type(presentation["enabled"]) is bool and presentation["enabled"] == gpu,
            "presentation enabled matches requested lane")
    require(type(presentation["ready"]) is bool, "presentation ready is boolean")
    for key in ("frame_count", "world_revision", "snapshot_sequence"):
        uint(presentation[key], "presentation " + key)
    if not gpu:
        compare(presentation, {"enabled": False, "ready": False, "frame_count": 0,
                              "world_revision": 0, "snapshot_sequence": 0}, "headless presentation")
    else:
        require(presentation["world_revision"] <= revision and
                presentation["snapshot_sequence"] <= value["snapshot_sequence"],
                "presented publication never leads the owner publication")
        require((presentation["frame_count"] == 0) == (presentation["snapshot_sequence"] == 0),
                "presented frame and publication have coherent presence")
        require(presentation["frame_count"] == 0 or presentation["ready"],
                "presented frame requires a ready renderer")
    if previous is not None:
        for key in ("simulation_tick", "snapshot_sequence", "overload_count", "dropped_ticks"):
            require(value[key] >= previous[key], "runtime observations monotonic " + key)
        for key in ("frame_count", "world_revision", "snapshot_sequence"):
            require(presentation[key] >= previous["presentation"][key],
                    "presentation observations monotonic " + key)
    return value
