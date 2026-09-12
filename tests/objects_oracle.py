#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Independent full-state and little-endian byte oracle for atomic objects."""
import argparse
import copy
import hashlib
import json
import math
from pathlib import Path
import shutil
import struct
import tempfile

import bootstrap_oracle as audit
from protocol_oracle import assert_json

SEED = 7
FIXTURES = Path(__file__).resolve().parent / "fixtures" / "objects"


class Evidence(audit.Evidence):
    def __init__(self, executable, directory):
        super().__init__(executable, directory)
        self.manifest.update(work_item="PR-003", example="objects.atomic",
            limitations=["Synchronous volatile authoring only; no authentication, asynchronous scheduling, physics, rendering, persistence, or runtime-tested uint64 exhaustion."])
    def retain(self, source, relative):
        if self.directory:
            target = self.directory / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(source, target)
            self.manifest["artifacts"][relative] = {
                "path": relative, "sha256": hashlib.sha256(target.read_bytes()).hexdigest()}


def encoded(value):
    return (json.dumps(value, ensure_ascii=False, allow_nan=False, separators=(",", ":"))+"\n").encode("utf-8")


def canonical(snapshot):
    def string(value):
        raw = value.encode("ascii")
        return struct.pack("<I", len(raw)) + raw
    data = bytearray(b"OWOBJ001")
    data.extend(string(snapshot["world_id"]))
    data.extend(struct.pack("<IIQI", snapshot["seed"], snapshot["max_slots"],
                            snapshot["world_revision"], len(snapshot["slots"])))
    for slot in snapshot["slots"]:
        data.extend(slot["entity_uuid"].encode("ascii"))
        data.extend(struct.pack("<QBB", slot["generation"], slot["retired"], slot["entity"] is not None))
        entity = slot["entity"]
        if entity is not None:
            data.extend(string(entity["prefab"]))
            data.extend(struct.pack("<Q", entity["authoring_revision"]))
            transform = entity["transform"]
            values = [*transform["position_m"], *transform["rotation_xyzw"], *transform["scale"]]
            data.extend(struct.pack("<10d", *(0.0 if value == 0 else value for value in values)))
    return bytes(data)


def identity(number, generation=1, world="workshop"):
    return {"world_id": world, "entity_uuid": f"00000007-0000-4000-8000-{number:012x}", "generation": generation}


def transform(position=(0, 0, 0), rotation=(0, 0, 0, 1), scale=(1, 1, 1)):
    return {"position_m": list(position), "rotation_xyzw": list(rotation), "scale": list(scale)}


def slot(number, revision, value=None, generation=1):
    return {"entity_uuid": identity(number)["entity_uuid"], "generation": generation, "retired": False,
            "entity": {"prefab": "builtin.unit_cube", "authoring_revision": revision,
                       "transform": copy.deepcopy(transform() if value is None else value)}}


def binding(name, number, generation=1):
    return {"temporary_id": name, **identity(number, generation)}


def create(name, prefab="builtin.unit_cube"):
    return {"type": "entity.create", "temporary_id": name, "prefab": prefab}


def move(target, position=(0, 0, 0), rotation=(0, 0, 0, 1), scale=(1, 1, 1)):
    return {"type": "transform.set", "target": copy.deepcopy(target), **transform(position, rotation, scale)}


def delete(target):
    return {"type": "entity.delete", "target": copy.deepcopy(target), "child_policy": "reject_if_children"}


def temporary(name):
    return {"temporary_id": name}


def empty(max_slots):
    return {"format_version": 1, "world_id": "workshop", "seed": SEED, "max_slots": max_slots,
            "world_revision": 0, "slots": []}


def scenarios(max_slots=4):
    cases = []
    current = empty(max_slots)
    def add(name, operations, after=None, code=None, path=None, operation_index=None,
            created=(), expected_revision=None, world="workshop"):
        sequence = len(cases)+1
        request = {"protocol_version": "0.1", "world_id": world,
            "transaction_id": f"018f7242-4387-7c98-a114-67787915a{300+sequence:03d}",
            "idempotency": {"epoch": "fixture-epoch", "sequence": sequence},
            "expected_world_revision": current["world_revision"] if expected_revision is None else expected_revision,
            "apply_at": {"mode": "next_tick", "expires_after_ticks": 120},
            "budget": {"max_operations": len(operations), "max_blob_bytes": 0},
            "operations": operations}
        cases.append({"name": name, "request": request, "after": copy.deepcopy(current if after is None else after),
                      "code": code, "path": path, "operation_index": operation_index,
                      "created": list(created)})
    initial = empty(max_slots)
    initial["world_revision"] = 1
    initial["slots"] = [slot(1, 1, transform((1,2,3), scale=(1,2,1))),
                        slot(2, 1, transform((-2,0,4), rotation=(0,0,1,0), scale=(-1,1,1)))]
    add("two-objects-one-commit",
        [create("A"), move(temporary("A"), (1,2,3), scale=(1,2,1)),
         create("B"), move(temporary("B"), (-2,0,4), rotation=(0,0,1,0), scale=(-1,1,1))],
        after=initial, created=[binding("A",1), binding("B",2)])
    current = copy.deepcopy(initial)
    add("staged-prefix-rollback",
        [move(identity(1), (99,98,97)), create("C"), delete(identity(2)), move(temporary("missing"))],
        code="NOT_FOUND", path="/operations/3/target/temporary_id", operation_index=3)
    recovered = copy.deepcopy(current)
    recovered["world_revision"] = 2
    recovered["slots"].append(slot(3, 2))
    add("create-after-rejection", [create("C")], after=recovered, created=[binding("C",3)])
    current = copy.deepcopy(recovered)
    add("stale-revision", [create("D")], code="REVISION_CONFLICT",
        path="/expected_world_revision", expected_revision=1)
    add("wrong-target-world", [move(identity(1), (9,9,9)), move(identity(2, world="elsewhere"))],
        code="NOT_FOUND", path="/operations/1/target*", operation_index=1)
    add("wrong-envelope-world", [create("D")], code="NOT_FOUND", path="/world_id", world="elsewhere")
    add("duplicate-temporary-name", [create("D"), create("D")],
        code="INVALID_SCHEMA", path="/operations/1/temporary_id", operation_index=1)
    add("forward-temporary-reference", [move(temporary("future")), create("future")],
        code="NOT_FOUND", path="/operations/0/target/temporary_id", operation_index=0)
    add("prior-transaction-temporary-reference", [move(identity(1), (9,9,9)), move(temporary("C"))],
        code="NOT_FOUND", path="/operations/1/target/temporary_id", operation_index=1)
    add("missing-prefab", [move(identity(1), (9,9,9)), create("D", "missing.cube")],
        code="NOT_FOUND", path="/operations/1/prefab", operation_index=1)
    add("zero-scale-rollback", [move(identity(1), (9,9,9)), move(identity(2), scale=(1,0,1))],
        code="INVALID_SCHEMA", path="/operations/1/scale", operation_index=1)
    add("zero-quaternion-rollback", [move(identity(1), (9,9,9)), move(identity(2), rotation=(0,0,0,0))],
        code="INVALID_SCHEMA", path="/operations/1/rotation_xyzw", operation_index=1)
    normalized_zero = copy.deepcopy(current)
    normalized_zero["world_revision"] = 3
    normalized_zero["slots"][0] = slot(1,3,transform((0,0,0), rotation=(0,0,0,1.0000000000002), scale=(-1,2,3)))
    add("negative-zero-and-quaternion-tolerance",
        [move(identity(1), (0.0,-0.0,0.0), rotation=(-0.0,0,0,1.0000000000002), scale=(-1,2,3))],
        after=normalized_zero)
    current = copy.deepcopy(normalized_zero)
    add("quaternion-outside-tolerance", [move(identity(2), rotation=(0,0,0,1.000000000001))],
        code="INVALID_SCHEMA", path="/operations/0/rotation_xyzw", operation_index=0)
    add("capacity-rollback", [create("D"), create("E")],
        code="BUDGET_EXCEEDED", path="/operations/1*", operation_index=1)
    deleted = copy.deepcopy(current)
    deleted["world_revision"] = 4
    deleted["slots"][1] = {"entity_uuid": identity(2)["entity_uuid"], "generation": 2, "retired": False, "entity": None}
    add("delete-B", [delete(identity(2))], after=deleted)
    current = copy.deepcopy(deleted)
    add("deleted-generation-before-reuse", [move(identity(1), (9,9,9)), move(identity(2))],
        code="STALE_HANDLE", path="/operations/1/target*", operation_index=1)
    add("provisional-reuse-does-not-revive-old-handle", [create("D"), move(identity(2))],
        code="STALE_HANDLE", path="/operations/1/target*", operation_index=1)
    reused = copy.deepcopy(current)
    reused["world_revision"] = 5
    reused["slots"][1] = slot(2, 5, transform((8,9,10)), generation=2)
    add("reuse-lowest-free-slot", [create("D"), move(temporary("D"), (8,9,10))],
        after=reused, created=[binding("D",2,2)])
    current = copy.deepcopy(reused)
    add("stale-generation-after-reuse", [move(identity(2,2), (9,9,9)), move(identity(2))],
        code="STALE_HANDLE", path="/operations/1/target*", operation_index=1)
    add("wrong-live-generation", [create("E"), move(identity(1,2))],
        code="STALE_HANDLE", path="/operations/1/target*", operation_index=1)
    add("unknown-uuid", [create("E"), move(identity(1024))],
        code="NOT_FOUND", path="/operations/1/target*", operation_index=1)
    add("temporary-binding-must-keep-generation",
        [create("E"), delete(temporary("E")), create("F"), move(temporary("E"), (99,98,97))],
        code="STALE_HANDLE", path="/operations/3/target*", operation_index=3)
    history = copy.deepcopy(current)
    history["world_revision"] = 6
    history["slots"].append(slot(4,6,generation=2))
    add("committed-bindings-include-deleted-create",
        [create("E"), delete(temporary("E")), create("F")],
        after=history, created=[binding("E",4,1),binding("F",4,2)])
    current = copy.deepcopy(history)
    add("deleted-temporary-name-remains-reserved",
        [delete(identity(4,2)),create("G"),delete(temporary("G")),create("G")],
        code="INVALID_SCHEMA", path="/operations/3/temporary_id", operation_index=3)
    noop = copy.deepcopy(current)
    noop["world_revision"] = 7
    noop["slots"][0]["entity"]["authoring_revision"] = 7
    add("authored-noop-still-commits",
        [move(identity(1), (0,0,0), rotation=(0,0,0,1.0000000000002), scale=(-1,2,3))], after=noop)
    current = copy.deepcopy(noop)
    final = copy.deepcopy(current)
    final["world_revision"] = 8
    final["slots"][3] = slot(4,8,generation=3)
    add("final-recovery", [delete(identity(4,2)),create("H")],
        after=final, created=[binding("H",4,3)])
    return cases


def check_state(name, actual, expected, actual_hex):
    assert_json(name, actual, expected)
    for item in actual["slots"]:
        if item["entity"] is not None:
            value = item["entity"]["transform"]
            for number in [*value["position_m"], *value["rotation_xyzw"], *value["scale"]]:
                audit.require(math.isfinite(number), name+": finite stored component")
                if number == 0:
                    audit.equal(name+": stored zero sign", math.copysign(1.0,number), 1.0)
    expected_bytes = canonical(expected)
    audit.equal(name+": canonical bytes", actual_hex, expected_bytes.hex())
    return hashlib.sha256(expected_bytes).hexdigest()


def inspect_report(payload, cases, max_slots):
    report = json.loads(payload)
    audit.equal("objects report fields", sorted(report), ["example","results","schema_version","seed","verified"])
    for key,value in {"schema_version":1,"example":"objects.atomic","seed":7,"verified":True}.items():
        assert_json("objects report "+key,report[key],value)
    audit.equal("objects one result per transaction",len(report["results"]),len(cases))
    before = empty(max_slots)
    for index,(case,result) in enumerate(zip(cases,report["results"])):
        name = case["name"]
        audit.equal(name+": result fields",sorted(result),
                    ["after","after_canonical_hex","before","before_canonical_hex","index","receipt"])
        audit.equal(name+": index",result["index"],index)
        check_state(name+": full-state before",result["before"],before,result["before_canonical_hex"])
        # State is asserted before receipt metadata so mutation failures name the actual rollback defect.
        check_state(name+": full-state after",result["after"],case["after"],result["after_canonical_hex"])
        receipt = result["receipt"]
        audit.equal(name+": receipt fields",sorted(receipt),
                    ["created","durability","errors","status","transaction_id","world_revision"])
        audit.equal(name+": receipt status",receipt["status"],"rejected" if case["code"] else "committed")
        audit.equal(name+": volatility",receipt["durability"],"volatile")
        audit.equal(name+": transaction identity",receipt["transaction_id"],case["request"]["transaction_id"])
        audit.equal(name+": receipt revision",receipt["world_revision"],case["after"]["world_revision"])
        assert_json(name+": creation history",receipt["created"],case["created"])
        if case["code"] is None:
            audit.equal(name+": no errors",receipt["errors"],[])
        else:
            audit.equal(name+": exactly one error",len(receipt["errors"]),1)
            error = receipt["errors"][0]
            keys = ["code","message","path"] + ([] if case["operation_index"] is None else ["operation_index"])
            audit.equal(name+": error fields",sorted(error),sorted(keys))
            audit.equal(name+": error code",error["code"],case["code"])
            if case["path"].endswith("*"):
                audit.require(error["path"].startswith(case["path"][:-1]),name+": error path prefix")
            else:
                audit.equal(name+": error path",error["path"],case["path"])
            if case["operation_index"] is not None:
                audit.equal(name+": operation index",error["operation_index"],case["operation_index"])
            audit.require(isinstance(error["message"],str) and bool(error["message"].strip()),name+": error explains rejection")
            assert_json(name+": rejected full state unchanged",result["after"],result["before"])
            audit.equal(name+": rejected bytes unchanged",result["after_canonical_hex"],result["before_canonical_hex"])
        before = case["after"]
    return report


def run(executable,root,cases,max_slots,evidence,name,builtin=False):
    output = root/name
    command = [str(executable),"--example","objects.atomic","--headless","--seed","7","--verify",
               "--output",str(output)]
    if not builtin:
        command.extend(["--max-slots",str(max_slots)])
        inputs = root/(name+"-inputs")
        inputs.mkdir()
        for index,case in enumerate(cases):
            path = inputs/(f"{index:02d}-"+case["name"]+".json")
            path.write_bytes(encoded(case["request"]))
            evidence.retain(path,name+"-inputs/"+path.name)
            command.extend(["--input",str(path)])
    result = audit.execute(command)
    audit.equal(name+": CLI exit",result.returncode,0)
    path = output/"result.json"
    payload = path.read_bytes()
    if name=="main":
        evidence.retain_result(payload)
    else:
        evidence.retain(path,name+"-result.json")
    report = inspect_report(payload,cases,max_slots)
    return report,payload,command


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable",required=True,type=Path)
    parser.add_argument("--evidence-dir",type=Path)
    parser.add_argument("--case-set",choices=("all","rollback"),default="all")
    args = parser.parse_args()
    executable = args.executable.resolve(strict=True)
    golden = json.loads((FIXTURES/"empty-canonical.json").read_text(encoding="utf-8"))
    audit.equal("independent encoder empty-state golden",canonical(golden["snapshot"]).hex(),golden["hex"])
    with Evidence(executable,args.evidence_dir) as evidence,tempfile.TemporaryDirectory(prefix="ow-objects-") as temp:
        directory = Path(temp)
        cases = scenarios()
        if args.case_set=="rollback":
            cases=cases[:3]
        report,payload,command = run(executable,directory,cases,4,evidence,"main")
        if args.case_set=="all":
            control_cases = [cases[0],cases[2]]
            control,_,_ = run(executable,directory,control_cases,4,evidence,"control")
            assert_json("allocation after rejection matches control receipt",
                        report["results"][2]["receipt"],control["results"][1]["receipt"])
            assert_json("allocation after rejection matches complete control state",
                        report["results"][2]["after"],control["results"][1]["after"])
            audit.equal("control canonical allocation state",report["results"][2]["after_canonical_hex"],
                        control["results"][1]["after_canonical_hex"])
            builtin_cases = scenarios(1024)[:3]
            _,first,builtin_command = run(executable,directory,builtin_cases,1024,evidence,"builtin",builtin=True)
            _,second,_ = run(executable,directory,builtin_cases,1024,evidence,"builtin-repeat",builtin=True)
            audit.equal("seed7 public fixture byte determinism",hashlib.sha256(second).hexdigest(),
                        hashlib.sha256(first).hexdigest())
            collision = audit.execute(builtin_command)
            audit.require(collision.returncode!=0,"objects output collision fails")
            audit.require((directory/"builtin"/"result.json").read_bytes()==first,
                          "objects output collision preserves earlier artifact bytes")
            absent_output=directory/"absent-output"
            absent=audit.execute([str(executable),"--example","objects.atomic","--headless","--seed","7",
                                 "--verify","--output",str(absent_output),"--input",str(directory/"missing.json")])
            audit.require(absent.returncode!=0,"missing objects input fails")
            audit.require(not absent_output.exists(),"missing objects input publishes no output")
        print(json.dumps({"status":"passed","transactions":len(cases),
                          "assertions":len(audit.ASSERTIONS),"final_canonical_sha256":
                          hashlib.sha256(canonical(cases[-1]["after"])).hexdigest()}))
    return 0


if __name__=="__main__":
    raise SystemExit(main())
