#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Independent literal filtered query results and tag diagnostic/transport oracle."""
import argparse
import copy
import math
import struct
import tempfile
from pathlib import Path
from hierarchy_oracle import (Evidence as PriorEvidence, Host, PolicySession, ROOT, CHECKS, Failure,
                              digest, encoded, exact, exchange, main_guard, request_bytes, require, strict_json,
                              unsupported_observation)


def identity(n):
    return {"world_id": "workshop", "entity_uuid": f"00000007-0000-4000-8000-{n:012x}", "generation": 1}


def trs(x=0.0, q=(0.,0.,0.,1.), scale=(1.,1.,1.)):
    return {"position_m": [float(x),0.,0.], "rotation_xyzw": list(q), "scale": list(scale)}


def item(n):
    # Literal geometry calculations, independent of the native fixture or bounds helper.
    t, labels, lower, upper = {
        2: (trs(8,(0.,0.,1.,0.),(2.,2.,2.)), ["chair","red"], [7.,-1.,-1.], [9.,1.,1.]),
        3: (trs(-3,scale=(-2.,1.,1.)), ["blue","chair"], [-4.,-.5,-.5], [-2.,.5,.5]),
        5: (trs(1,(0.,0.,.6,.8),(2.,1.,1.)), ["red","table"], [.24,-1.1,-.5], [1.76,1.1,.5]),
        6: (trs(5), ["chair","red"], [4.5,-.5,-.5], [5.5,.5,.5]),
    }[n]
    return {"entity_uuid": identity(n)["entity_uuid"], "generation": 1, "authoring_revision": 1,
            "tags": labels, "transform": t, "bounds": {"lower": lower, "upper": upper}}


def expected():
    specs = [("red-first",1,[2,5],True), ("retained-tail",1,[6],False), ("replayed-tail",1,[6],False),
             ("fresh-red",2,[2,5],False), ("replaced-cursor", "REQUIRES_RESYNC"),
             ("chairs",2,[2,3],False), ("inclusive-contact",2,[3],False), ("combined",2,[2],False),
             ("deny-all",2,[],False), ("full-grant-containment",2,[2,5],False),
             ("response-budget","BUDGET_EXCEEDED"), ("default-cursor","REQUIRES_RESYNC"),
             ("foreign-cursor","REQUIRES_RESYNC"), ("expiry-first",2,[2],True),
             ("expired-cursor","REQUIRES_RESYNC"), ("expiry-recovery",2,[2],True),
             ("closed-cursor","REQUIRES_RESYNC"), ("closed-start","NOT_AUTHORIZED")]
    rows = []
    for spec in specs:
        label = spec[0]
        if len(spec) == 2:
            rows.append({"case": label, "code": spec[1], "page": None})
        else:
            rows.append({"case": label, "code": "", "page": {"schema_version": 1, "world_id": "workshop",
                "world_revision": spec[1], "items": [item(n) for n in spec[2]], "has_more": spec[3]}})
    return {"schema_version": 1, "example": "observe.semantic_query", "seed": 7, "verified": True,
            "state_unchanged_by_queries": True, "records": rows}


def compare(actual, wanted, label):
    if type(wanted) is dict:
        require(type(actual) is dict and actual.keys() == wanted.keys(), label + ": exact fields")
        for key in wanted: compare(actual[key], wanted[key], label + "/" + key)
    elif type(wanted) is list:
        require(type(actual) is list and len(actual) == len(wanted), label + ": exact length")
        for n, value in enumerate(wanted): compare(actual[n], value, label + "/" + str(n))
    elif type(wanted) is float:
        require(type(actual) in (int,float) and math.isfinite(actual) and abs(actual-wanted) <= 1e-10,
                label + ": preregistered geometry tolerance")
    else:
        require(type(actual) is type(wanted) and actual == wanted, label + ": exact type/value")


def validate(value):
    compare(value, expected(), "independent filtered query fixture")


def tag_states():
    def state(revision, version, slots):
        return {"format_version":version,"world_id":"workshop","seed":7,"max_slots":2,"world_revision":revision,"slots":slots}
    def slot(n, revision):
        return {"entity_uuid":identity(n)["entity_uuid"],"generation":1,"retired":False,
                "entity":{"prefab":"builtin.unit_cube","authoring_revision":revision,"transform":trs()}}
    tagged=slot(1,1);tagged["entity"].update(parent=None,local_transform=None,tags=["a","z"])
    first=state(1,3,[tagged])
    return [state(0,1,[]),first,copy.deepcopy(first),state(2,1,[slot(1,2)]),state(3,1,[slot(1,2),slot(2,3)])]


def canonical(state):
    def text(s):
        b=s.encode("ascii");return struct.pack("<I",len(b))+b
    data=bytearray(b"OWOBJ003" if state["format_version"] == 3 else b"OWOBJ001")+text(state["world_id"])
    data+=struct.pack("<IIQI",state["seed"],state["max_slots"],state["world_revision"],len(state["slots"]))
    for slot in state["slots"]:
        data+=slot["entity_uuid"].encode("ascii")+struct.pack("<QBB",slot["generation"],slot["retired"],True)
        e=slot["entity"];t=e["transform"]
        data+=text(e["prefab"])+struct.pack("<Q",e["authoring_revision"])
        data+=struct.pack("<10d",*(t["position_m"]+t["rotation_xyzw"]+t["scale"]))
        if state["format_version"] == 3:
            # This independent diagnostic fixture contains roots, so the parent flag is zero.
            data+=b"\0"+struct.pack("<I",len(e["tags"]))
            for tag in e["tags"]: data+=text(tag)
    return bytes(data)


def tag_fixture(executable,evidence,directory):
    create=lambda name:{"type":"entity.create","temporary_id":name,"prefab":"builtin.unit_cube"}
    labels=lambda target,tags:{"type":"entity.tags.set","target":target,"tags":tags}
    operations=[[create("A"),labels({"temporary_id":"A"},["z","a"])],
                [labels(identity(1),["changed"]),create("B"),labels({"temporary_id":"missing"},["x"])],
                [labels(identity(1),[])],[create("B")]]
    states=tag_states();inputs=[]
    for n,ops in enumerate(operations):
        request={"protocol_version":"0.1","world_id":"workshop","transaction_id":f"018f7242-4387-7c98-a114-67787915a38{n}",
                 "idempotency":{"epoch":"fixture-epoch","sequence":n+1},"expected_world_revision":states[n]["world_revision"],
                 "apply_at":{"mode":"next_tick","expires_after_ticks":120},"budget":{"max_operations":len(ops),"max_blob_bytes":0},"operations":ops}
        path=directory/f"input-{n}.json";path.write_bytes(encoded(request));inputs.extend(["--input",str(path)])
    tail=["--example","objects.atomic","--headless","--seed","7","--verify","--max-slots","2","--output"]
    output=directory/"tags"
    result=evidence.run([str(executable),*tail,str(output),*inputs],["<examples>",*tail,"<output>","--input","<four-native-fixtures>"])
    require(result.returncode==0,"typed tag diagnostic fixture executes")
    report=strict_json((output/"result.json").read_bytes())
    exact({k:report[k] for k in report if k!="results"},{"schema_version":1,"example":"objects.atomic","seed":7,"verified":True},"tag report fields")
    require(type(report["results"]) is list and len(report["results"])==4,"complete tag transaction sequence")
    for n,row in enumerate(report["results"]):
        exact(set(row),{"index","receipt","before","after","before_canonical_hex","after_canonical_hex"},"tag record fields")
        exact(row["index"],n,"tag record index");compare(row["before"],states[n],"tag full before");compare(row["after"],states[n+1],"tag full after")
        exact(row["before_canonical_hex"],canonical(states[n]).hex(),"independent tag bytes before")
        exact(row["after_canonical_hex"],canonical(states[n+1]).hex(),"independent tag bytes after")
        errors=[{"code":"NOT_FOUND","path":"/operations/2/target/temporary_id",
                 "message":"Temporary target must reference an earlier create in this batch.","operation_index":2}] if n==1 else []
        created=[{"temporary_id":"A" if n==0 else "B",**identity(1 if n==0 else 2)}] if n in (0,3) else []
        exact(row["receipt"],{"status":"rejected" if n==1 else "committed","durability":"volatile",
              "transaction_id":f"018f7242-4387-7c98-a114-67787915a38{n}","world_revision":states[n+1]["world_revision"],
              "created":created,"errors":errors},"tag receipt and allocator recovery")
    evidence.retain_json("tags/actual.json",report)
    evidence.retain_json("tags/expected-states.json",states)


class Evidence(PriorEvidence):
    def __init__(self,directory,executable):
        super().__init__(directory,executable)
        for name in ("observe_oracle.py","observe_oracle_test.py","observe_native_test.cpp"):
            p=ROOT/"tests"/name;self.sources[p.relative_to(ROOT).as_posix()]=digest(p.read_bytes())
        self.manifest.update(work_item="PR-010",example="observe.semantic_query",source_sha256=self.sources,
             limitations=["Native typed queries only; no remote query endpoint or wire cursor.",
                          "Host-issued immutable grant and World must outlive its single-owner session.",
                          "Projected payload caps are not a global service/RSS bound; no GPU or physics evidence."])


def tag_gate(descriptor,evidence,label,policy=False):
    evidence.register(descriptor)
    def call(method,route,body=None):
        return exchange(descriptor,request_bytes(descriptor,method,route,None if body is None else encoded(body)))
    observe={"protocol_version":"0.1","world_id":"workshop"}
    before=call("POST","/v0/observe",observe)
    require(before is not None and before[0]==200,"tag gate initial observation")
    require(before[1]["snapshot"]["slots"]==[] and before[1]["next_sequence"]==1,"tag gate empty fixture")
    create={"type":"entity.create","temporary_id":"A","prefab":"builtin.unit_cube"}
    place={"type":"transform.set","target":{"temporary_id":"A"},**trs(3)}
    tags={"type":"entity.tags.set","target":{"temporary_id":"A"},"tags":["red"]}
    base={"protocol_version":"0.1","world_id":"workshop","transaction_id":"018f7242-4387-7c98-a114-67787915a391",
          "idempotency":{"epoch":descriptor["epoch"],"sequence":1},"expected_world_revision":0,
          "apply_at":{"mode":"next_tick","expires_after_ticks":120},"budget":{"max_operations":3,"max_blob_bytes":0}}
    proof=[]
    for ops in ([tags],[create,place,tags]):
        response=call("POST","/v0/transactions",{**base,"operations":ops})
        exact(response,(405,{"protocol_version":"0.1","status":"rejected","error":{"code":"UNSUPPORTED_OPERATION","path":"/operations"}}),"complete tag transport denial")
        exact(call("POST","/v0/observe",observe),before,"tag denial consumes no state or sequence")
        proof.append({"operations":len(ops),"code":"UNSUPPORTED_OPERATION","revision":0,"next_sequence":1})
    recovery=call("POST","/v0/transactions",{**base,"operations":[create,place]})
    require(recovery is not None and recovery[0]==200,"same-sequence normal recovery")
    exact(recovery[1]["receipt"],{"status":"committed","durability":"volatile","transaction_id":base["transaction_id"],
          "world_revision":1,"created":[{"temporary_id":"A",**identity(1)}],"errors":[]},"tag rejection allocator recovery")
    exact(recovery[1]["next_sequence"],2,"one admitted sequence")
    if policy:
        status=call("GET","/v0/policy");require(status is not None and status[0]==200,"policy remains available")
        exact(status[1]["policy"]["usage"],{"retained_bytes":384,"working_bytes":0,"requests":0,"global_requests":0},"tag denial refunds policy resources")
        evidence.retain_json(label+"/usage.json",status[1]["policy"]["usage"])
    evidence.retain_json(label+"/denials.json",proof)
    evidence.retain_json(label+"/recovery.json",recovery[1]["receipt"])


def main():
    parser=argparse.ArgumentParser()
    for name in ("executable","control","agents","policy","tag-legacy","tag-policy"):
        parser.add_argument("--"+name,required=True,type=Path)
    parser.add_argument("--native-test",type=Path);parser.add_argument("--evidence",type=Path)
    args=parser.parse_args();evidence=Evidence(args.evidence,args.executable)
    try:
        for name in ("executable","control","agents","policy","tag_legacy","tag_policy","native_test"):
            path=getattr(args,name)
            if path:evidence.bind_executable(name,path)
        with tempfile.TemporaryDirectory(prefix="ow-query-") as temporary:
            root=Path(temporary);output=root/"query"
            tail=["--example","observe.semantic_query","--headless","--seed","7","--verify","--output"]
            r=evidence.run([str(args.executable),*tail,str(output)],["<examples>",*tail,"<output>"])
            require(r.returncode==0,"runnable native query example executes")
            report=strict_json((output/"result.json").read_bytes());validate(report)
            evidence.retain_json("query/actual.json",report);evidence.retain_json("query/expected.json",expected())
            tag_fixture(args.executable,evidence,root)
        if args.native_test:
            result=evidence.run([str(args.native_test)],["<observe_native_test>"])
            require(result.returncode==0,"independent native query assertions passed")
            summary=strict_json(result.stdout.encode("utf-8"))
            require(type(summary) is dict and set(summary)=={"status","assertions"} and summary["status"]=="passed" and
                    type(summary["assertions"]) is int and summary["assertions"]>=100,"native query allowlisted summary")
            evidence.retain_json("query/native-assertions.json",summary)
        for label,path in (("control",args.control),("agents",args.agents)):
            with Host(path,evidence) as host:tag_gate(host.descriptor,evidence,label)
        with PolicySession(args.policy) as session:
            info=session.client("east")._info
            tag_gate({"port":info.port,"token":info.token,"epoch":info.epoch},evidence,"policy",True)
        with Host(args.tag_legacy,evidence) as host:unsupported_observation(host.descriptor,evidence,"legacy_tags")
        with PolicySession(args.tag_policy) as session:
            info=session.client("east")._info
            unsupported_observation({"port":info.port,"token":info.token,"epoch":info.epoch},evidence,"policy_tags",True)
        evidence.finish("passed");print("query oracle passed: "+str(len(evidence.manifest["assertions"]))+" assertions");return 0
    except Failure as error:evidence.finish("failed",str(error));raise
    except Exception:evidence.finish("failed","unexpected private internal error");raise


if __name__=="__main__":raise SystemExit(main_guard(main))
