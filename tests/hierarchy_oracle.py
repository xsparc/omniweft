#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Independent literal hierarchy states, diagnostic bytes and real transport gates."""
import argparse
import copy
import struct
import sys
import tempfile
from pathlib import Path
from sdk_test_support import (Evidence as BaseEvidence, Host, ROOT, CHECKS, Failure, digest,
                              encoded, exact, exchange, main_guard, request_bytes, require, strict_json)
sys.path.insert(0, str(ROOT / "sdk/python"))
from omniweft_sdk import PolicySession


def identity(number, generation=1):
    return {"world_id": "workshop", "entity_uuid": f"00000007-0000-4000-8000-{number:012x}", "generation": generation}


def transform(x=0, q=(0, 0, 0, 1), scale=1):
    return {"position_m": [float(x), 0.0, 0.0], "rotation_xyzw": [float(v) for v in q], "scale": [float(scale)] * 3}


def slot(number, revision, value, parent=None, local=None, hierarchy=True):
    entity = {"prefab": "builtin.unit_cube", "authoring_revision": revision, "transform": value}
    if hierarchy:
        entity.update(parent=None if parent is None else {k: v for k, v in identity(parent).items() if k != "world_id"},
                      local_transform=local)
    return {"entity_uuid": identity(number)["entity_uuid"], "generation": 1, "retired": False, "entity": entity}


def snapshot(revision=0, slots=(), version=1):
    return {"format_version": version, "world_id": "workshop", "seed": 7, "max_slots": 8,
            "world_revision": revision, "slots": list(slots)}


def fixture_states():
    # Z half-turn with uniform scale2: local(1,0,0) becomes world(8,0,0).
    p = slot(1, 1, transform(10, (0,0,1,0), 2))
    c = slot(2, 1, transform(8, (0,0,1,0), 2), 1, transform(1))
    first = snapshot(1, [p, c], 2)
    # Moving to Q at -4 with preserve_world yields local12, unchanged world8.
    second = snapshot(2, [copy.deepcopy(p), slot(2, 2, transform(8, (0,0,1,0), 2), 3,
        transform(12, (0,0,1,0), 2)), slot(3, 2, transform(-4))], 2)
    third = snapshot(3, [copy.deepcopy(p), slot(2, 3, transform(10, (0,0,1,0), 2), 3,
        transform(12, (0,0,1,0), 2)), slot(3, 3, transform(-2))], 2)
    fourth = snapshot(4, [slot(1, 1, transform(10, (0,0,1,0), 2), hierarchy=False),
        slot(2, 4, transform(12, (0,0,1,0), 2), hierarchy=False),
        slot(3, 3, transform(-2), hierarchy=False)])
    return [snapshot(), first, second, copy.deepcopy(second), third, fourth]


def canonical(state):
    """Independently encode the documented OWOBJ001 / OWOBJ002 layouts."""
    def text(value):
        data = value.encode("ascii")
        return struct.pack("<I", len(data)) + data
    def trs(value):
        values = value["position_m"] + value["rotation_xyzw"] + value["scale"]
        return struct.pack("<10d", *(0.0 if v == 0 else v for v in values))
    result = bytearray(b"OWOBJ002" if state["format_version"] == 2 else b"OWOBJ001")
    result += text(state["world_id"])
    result += struct.pack("<IIQI", state["seed"], state["max_slots"], state["world_revision"], len(state["slots"]))
    for entry in state["slots"]:
        result += entry["entity_uuid"].encode("ascii")
        result += struct.pack("<QBB", entry["generation"], entry["retired"], entry["entity"] is not None)
        e = entry["entity"]
        if e is not None:
            result += text(e["prefab"]) + struct.pack("<Q", e["authoring_revision"]) + trs(e["transform"])
            if state["format_version"] == 2:
                result += struct.pack("<B", e["parent"] is not None)
                if e["parent"] is not None:
                    result += e["parent"]["entity_uuid"].encode("ascii") + struct.pack("<Q", e["parent"]["generation"])
                    result += trs(e["local_transform"])
    return bytes(result)


def expected_report():
    states = fixture_states()
    rows = []
    for i in range(5):
        creates = [("P", 1), ("C", 2)] if i == 0 else [("Q", 3)] if i == 1 else []
        receipt = {"status": "rejected" if i == 2 else "committed", "durability": "volatile",
            "transaction_id": f"018f7242-4387-7c98-a114-67787915a30{i+1}",
            "world_revision": states[i+1]["world_revision"],
            "created": [{"temporary_id": name, **identity(n)} for name, n in creates],
            "errors": [{"code": "INVALID_SCHEMA", "path": "/operations/0/parent",
                        "message": "Reparenting must not create a cycle.", "operation_index": 0}] if i == 2 else []}
        rows.append({"index": i, "receipt": receipt, "before": states[i], "after": states[i+1],
                     "before_canonical_hex": canonical(states[i]).hex(), "after_canonical_hex": canonical(states[i+1]).hex()})
    return {"schema_version": 1, "example": "objects.hierarchy", "seed": 7, "verified": True, "results": rows}


def validate_report(report):
    exact(report, expected_report(), "independent complete hierarchy fixture")


class Evidence(BaseEvidence):
    def __init__(self, directory, executable):
        super().__init__(directory, executable)
        files = [ROOT / "CMakeLists.txt", ROOT / "CMakePresets.json", ROOT / "schemas/protocol-0.1.schema.json"]
        files += sorted((ROOT / "src").glob("*.*")) + sorted((ROOT / "include/omniweft").glob("*.hpp"))
        files += [ROOT / "tests" / name for name in ("hierarchy_oracle.py", "hierarchy_oracle_test.py",
                  "hierarchy_native_test.cpp", "hierarchy_host_fixture.cpp", "render_test_support.py", "sdk_test_support.py")]
        self.sources = {p.relative_to(ROOT).as_posix(): digest(p.read_bytes()) for p in files}
        self.manifest.update(work_item="PR-009", example="objects.hierarchy", source_sha256=self.sources,
            limitations=["Native hierarchy only; all remote and policy hierarchy operations are unsupported.",
                         "CPU packet evidence is not physical GPU or physics evidence.",
                         "Diagnostic bytes only; no save parser, migration or persistence."])
        self.executables = {}

    def bind_executable(self, label, path):
        self.executables[label] = (Path(path), digest(Path(path).read_bytes()))
        self.manifest.setdefault("executable_hashes", {})[label] = self.executables[label][1]
        if self.directory:
            self.manifest.setdefault("builds", {})[label] = self._build_provenance(Path(path), "cpu")

    def finish(self, status, failure=None):
        if status == "passed":
            require(all(digest((ROOT / p).read_bytes()) == sha for p, sha in self.sources.items()), "all hierarchy sources remain stable")
            require(all(digest(p.read_bytes()) == sha for p, sha in self.executables.values()), "all tested executables remain stable")
        super().finish(status, failure)


def call(descriptor, method, route, body=None):
    return exchange(descriptor, request_bytes(descriptor, method, route, None if body is None else encoded(body)))


def remote_gate(descriptor, evidence, label, policy=False):
    # Descriptor and authenticated request/response bytes stay private; only allowlisted proof is retained.
    evidence.register(descriptor)
    observe = {"protocol_version": "0.1", "world_id": "workshop"}
    before = call(descriptor, "POST", "/v0/observe", observe)
    require(before is not None and before[0] == 200, "remote initial observation")
    exact(before[1]["snapshot"], snapshot(), "remote initially empty fixture")
    exact(before[1]["next_sequence"], 1, "initial sequence")
    caps = call(descriptor, "GET", "/v0/capabilities")
    require(caps is not None and caps[0] == 200, "capabilities available")
    exact(caps[1]["operations"], ["entity.create", "transform.set", "entity.delete"], "remote operations unchanged")
    create = {"type": "entity.create", "temporary_id": "C", "prefab": "builtin.unit_cube"}
    place = {"type": "transform.set", "target": {"temporary_id": "C"}, **transform(3)}
    hierarchy = {"type": "entity.reparent", "target": {"temporary_id": "C"}, "parent": None, "mode": "preserve_world"}
    base = {"protocol_version": "0.1", "world_id": "workshop", "transaction_id": "018f7242-4387-7c98-a114-67787915a391",
        "idempotency": {"epoch": descriptor["epoch"], "sequence": 1}, "expected_world_revision": 0,
        "apply_at": {"mode": "next_tick", "expires_after_ticks": 120}, "budget": {"max_operations": 3, "max_blob_bytes": 0}}
    proof = []
    for ops in ([hierarchy], [create, place, hierarchy]):
        denied = call(descriptor, "POST", "/v0/transactions", {**base, "operations": ops})
        exact(denied, (405, {"protocol_version": "0.1", "status": "rejected",
                            "error": {"code": "UNSUPPORTED_OPERATION", "path": "/operations"}}), "complete remote hierarchy denial")
        after = call(descriptor, "POST", "/v0/observe", observe)
        exact(after, before, "remote denial changes neither sequence nor complete state")
        proof.append({"operations": len(ops), "status": denied[0], "error": denied[1]["error"],
                      "world_revision": 0, "next_sequence": 1})
    accepted = call(descriptor, "POST", "/v0/transactions", {**base, "operations": [create, place]})
    require(accepted is not None and accepted[0] == 200, "same-sequence supported recovery")
    exact(accepted[1]["receipt"], {"status": "committed", "durability": "volatile", "transaction_id": base["transaction_id"],
        "world_revision": 1, "created": [{"temporary_id": "C", **identity(1)}], "errors": []}, "recovery allocation and receipt")
    exact(accepted[1]["next_sequence"], 2, "exactly one admitted sequence")
    after = call(descriptor, "POST", "/v0/observe", observe)
    require(after is not None and after[0] == 200, "recovery observable")
    exact(after[1]["snapshot"], snapshot(1, [slot(1,1,transform(3),hierarchy=False)]), "recovery complete root state")
    if policy:
        status = call(descriptor, "GET", "/v0/policy")
        require(status is not None and status[0] == 200, "policy status after hierarchy denial")
        exact(status[1]["policy"]["usage"], {"retained_bytes": 384, "working_bytes": 0, "requests": 0, "global_requests": 0},
              "denied and supported requests release all working resources")
        evidence.retain_json(label + "/usage.json", status[1]["policy"]["usage"])
    evidence.retain_json(label + "/denials.json", proof)
    evidence.retain_json(label + "/recovery.json", after[1]["snapshot"])


def unsupported_observation(descriptor, evidence, label, policy=False):
    evidence.register(descriptor)
    caps = call(descriptor, "GET", "/v0/capabilities")
    require(caps is not None and caps[0] == 200, "callback hierarchy fixture capabilities")
    # The deliberately unsupported native world is revision1; the untouched
    # policy ledger remains revision0 and its control metadata must say so.
    exact(caps[1]["world_revision"], 0 if policy else 1, "callback fixture reports its owning revision domain")
    exact(caps[1]["next_sequence"], 1, "hierarchy fixture initial sequence")
    proof = []
    for route in ("/v0/observe", "/v0/runtime"):
        response = call(descriptor, "POST" if route.endswith("observe") else "GET", route,
                        {"protocol_version": "0.1", "world_id": "workshop"} if route.endswith("observe") else None)
        exact(response, (405, {"protocol_version": "0.1", "status": "rejected",
                             "error": {"code": "UNSUPPORTED_OPERATION", "path": "/snapshot"}}),
              "hierarchy callback cannot publish lossy remote state")
        proof.append({"route": route, "status": 405, "code": "UNSUPPORTED_OPERATION"})
    exact(call(descriptor, "GET", "/v0/capabilities"), caps, "observation rejection preserves remote state and sequence")
    evidence.retain_json(label + "/unsupported_observation.json", proof)


def main():
    parser = argparse.ArgumentParser()
    for name in ("executable", "control", "agents", "policy", "hierarchy-legacy", "hierarchy-policy"):
        parser.add_argument("--" + name, required=True, type=Path)
    parser.add_argument("--native-test", type=Path)
    parser.add_argument("--evidence", type=Path)
    args = parser.parse_args()
    evidence = Evidence(args.evidence, args.executable)
    try:
        for name in ("executable", "control", "agents", "policy", "native_test", "hierarchy_legacy", "hierarchy_policy"):
            path = getattr(args, name)
            if path: evidence.bind_executable(name, path)
        with tempfile.TemporaryDirectory(prefix="ow-hierarchy-") as temporary:
            output = Path(temporary) / "fixture"
            tail = ["--example", "objects.hierarchy", "--headless", "--seed", "7", "--verify", "--max-slots", "8", "--output"]
            result = evidence.run([str(args.executable), *tail, str(output)], ["<examples>", *tail, "<output>"])
            require(result.returncode == 0, "hierarchy runnable example succeeds")
            report = strict_json((output / "result.json").read_bytes())
            validate_report(report)
            evidence.retain_json("native/fixture.json", report)
            evidence.retain_json("native/expected.json", expected_report())
        if args.native_test:
            result = evidence.run([str(args.native_test)], ["<hierarchy_native_test>"])
            require(result.returncode == 0, "independent native hierarchy suite succeeds")
            summary = strict_json(result.stdout.encode("utf-8"))
            require(type(summary) is dict and set(summary) == {"status", "assertions"} and summary["status"] == "passed" and
                    type(summary["assertions"]) is int and summary["assertions"] >= 100, "native suite bounded summary")
            evidence.retain_json("native/assertions.json", summary)
        for label, path in (("control", args.control), ("agents", args.agents)):
            with Host(path, evidence) as host:
                remote_gate(host.descriptor, evidence, label)
        with PolicySession(args.policy) as session:
            client = session.client("east")
            info = client._info
            remote_gate({"port": info.port, "token": info.token, "epoch": info.epoch}, evidence, "policy", True)
        with Host(args.hierarchy_legacy, evidence) as host:
            unsupported_observation(host.descriptor, evidence, "legacy_hierarchy")
        with PolicySession(args.hierarchy_policy) as session:
            info = session.client("east")._info
            unsupported_observation({"port": info.port, "token": info.token, "epoch": info.epoch}, evidence, "policy_hierarchy", True)
        evidence.finish("passed")
        print("hierarchy oracle passed: " + str(len(evidence.manifest["assertions"])) + " assertions")
        return 0
    except Failure as error:
        evidence.finish("failed", str(error)); raise
    except Exception:
        evidence.finish("failed", "unexpected private internal error"); raise


if __name__ == "__main__":
    raise SystemExit(main_guard(main))
