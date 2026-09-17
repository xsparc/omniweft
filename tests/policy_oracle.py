#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Independent literal fixture and real transport resource/recovery oracle."""
import argparse
import socket
import sys
import tempfile
import time
from pathlib import Path
from sdk_test_support import (Evidence, Failure, ROOT, canonical, encoded, exact, exchange,
                              main_guard, parse_response, request_bytes, require, strict_json)
sys.path.insert(0,str(ROOT/"sdk/python"))
from omniweft_sdk import CreateCube, PolicySession, SetTransform, TemporaryTarget, Transform


def validate(value):
    exact(set(value),{"schema_version","example","seed","records"},"fixture fields")
    exact((value["schema_version"],value["example"],value["seed"]),(1,"policy.denied_edits",7),"fixture identity")
    rows=value["records"]
    kinds=(["profile"]*2+["receipt"]*2+["snapshot"]+["receipt"]*8+["snapshot"]+
           ["http_rejection"]*3+["receipt"]*2+["usage"]*2+["snapshot"]+["renewed"]*2+["receipt","snapshot"])
    exact([r["kind"] for r in rows],kinds,"complete scenario order")
    for row in rows:
        keys={"profile":{"kind","principal","world_revision","usage","limits"},
              "receipt":{"kind","value"},"snapshot":{"kind","value"},
              "http_rejection":{"kind","case","code"},"usage":{"kind","principal","value"},
              "renewed":{"kind","principal","value","world_revision","next_sequence"}}
        exact(set(row),keys[row["kind"]],"every artifact row has only allowlisted fields")
    zero={"retained_bytes":0,"working_bytes":0,"requests":0,"global_requests":0}
    for row,name,retained,working,observation in ((rows[0],"west",512,98304,512),(rows[1],"east",2048,262144,4096)):
        exact(row,{"kind":"profile","principal":name,"world_revision":0,"usage":zero,
          "limits":{"max_operations":4,"max_body_bytes":16384,"retained_bytes":retained,
                    "working_bytes":working,"observation_bytes":observation,"requests":1,"global_requests":2}},
          "literal independently specified grant")
    receipts=[r["value"] for r in rows if r["kind"]=="receipt"]
    revisions=[1,2]+[2]*8+[3,4,5]
    codes=[None,None,"NOT_AUTHORIZED","NOT_AUTHORIZED","BUDGET_EXCEEDED"]+["NOT_FOUND"]*5+[None]*3
    for n,(receipt,revision,code) in enumerate(zip(receipts,revisions,codes),1):
        exact(set(receipt),{"status","durability","transaction_id","world_revision","created","errors"},"receipt fields")
        exact(receipt["transaction_id"],f"018f7242-4387-7c98-a114-{n:012x}","literal transaction correlation")
        exact((receipt["world_revision"],receipt["durability"],receipt["status"]),
              (revision,"volatile","rejected" if code else "committed"),"authoring and rejection oracle")
        if code:
            exact(receipt["created"],[],"rejected prefix has no bindings")
            path,message,operation=("/scope","Complete cube bounds must fit the host-issued write region.",0)
            if code=="BUDGET_EXCEEDED":
                path,message,operation=("/memory/retained","Staging prefix exceeds retained resource allowance.",0)
            elif code=="NOT_FOUND":
                path,message,operation=("/operations/1/target/temporary_id","Temporary target must reference an earlier create in this batch.",1)
            exact(receipt["errors"],[{"code":code,"path":path,"message":message,"operation_index":operation}],
                  "literal rejection fields exclude arbitrary server metadata")
        else:
            exact(receipt["errors"],[],"commit has no errors")
    for index,name,slot,generation in ((0,"A",1,1),(1,"B",2,1),(11,"C",1,2),(12,"D",3,1)):
        exact(receipts[index]["created"],[{"temporary_id":name,"world_id":"workshop",
            "entity_uuid":f"00000007-0000-4000-8000-{slot:012x}","generation":generation}],"literal allocator recovery")
    exact(receipts[10]["created"],[],"delete has no created binding")
    def slot(n,generation,revision,x):
        return {"entity_uuid":f"00000007-0000-4000-8000-{n:012x}","generation":generation,"retired":False,
                "entity":{"prefab":"builtin.unit_cube","authoring_revision":revision,
                "transform":{"position_m":[float(x),0.0,0.0],"rotation_xyzw":[0.0,0.0,0.0,1.0],"scale":[1.0,1.0,1.0]}}}
    def snapshot(revision,slots):
        return {"format_version":1,"world_id":"workshop","seed":7,"max_slots":8,"world_revision":revision,"slots":slots}
    expected=[snapshot(2,[slot(1,1,1,-3),slot(2,1,2,3)])]*2
    expected += [snapshot(4,[slot(1,2,4,5),slot(2,1,2,3)]),
                 snapshot(5,[slot(1,2,4,5),slot(2,1,2,3),slot(3,1,5,-3)])]
    snapshots=[r["value"] for r in rows if r["kind"]=="snapshot"]
    exact(snapshots,expected,"independent complete authoring state")
    exact(canonical(snapshots[0]),canonical(snapshots[1]),"all rejection prefixes preserve canonical bytes")
    exact([r for r in rows if r["kind"]=="http_rejection"],
          [{"kind":"http_rejection","case":case,"code":"BUDGET_EXCEEDED"}
           for case in ("operations","observation","runtime")],"public limits deny success")
    for kind in ("usage","renewed"):
        for row,name,retained in zip([r for r in rows if r["kind"]==kind],("west","east"),(0,768)):
            exact((row["principal"],row["value"]),(name,{**zero,"retained_bytes":retained}),"quota sponsor transfer and renewal")
            if kind=="renewed":
                exact((row["world_revision"],row["next_sequence"]),(4,1),"renewal retains world and resets sequence")
    return snapshots


def desc(client):
    info=client._info
    return {"port":info.port,"token":info.token,"epoch":info.epoch}


def denied(response,path):
    require(response is not None and response[0]==413,"bounded resource rejection HTTP status")
    exact(response[1],{"protocol_version":"0.1","status":"rejected",
          "error":{"code":"BUDGET_EXCEEDED","path":path}},"exact secret-free resource rejection")


def wait_status(client,predicate):
    deadline=time.monotonic()+2
    while time.monotonic()<deadline:
        status=client.policy_status()
        if predicate(status): return status
        time.sleep(.005)
    raise Failure("policy lease state did not converge")


def headers_only(payload):
    return payload.split(b"\r\n\r\n",1)[0]+b"\r\n\r\n"


def exchange_sized(descriptor,payload):
    raw=bytearray()
    with socket.create_connection(("127.0.0.1",descriptor["port"]),timeout=3) as connection:
        connection.settimeout(3);connection.sendall(payload)
        while True:
            chunk=connection.recv(4096)
            if not chunk: break
            raw+=chunk
            require(len(raw)<=16384,"exact-boundary wire response has bounded private storage")
    response=parse_response(bytes(raw))
    require(response is not None,"exact-boundary response exists")
    return response,len(raw.split(b"\r\n\r\n",1)[1])


def exact_limits(executable,evidence):
    with PolicySession(executable,max_runtime_ms=10000) as session:
        west,east=session.client("west"),session.client("east")
        a,b=desc(west),desc(east);evidence.register(a);evidence.register(b)
        transform=Transform(position_m=(-3.123456789012345,0.123456789012345,0.1234567890123))
        receipt=west.transact([CreateCube("A"),SetTransform(TemporaryTarget("A"),Transform(position_m=(-3,0,0))),
                              SetTransform(TemporaryTarget("A"),Transform(position_m=(-4,0,0))),
                              SetTransform(TemporaryTarget("A"),transform)],0,
                             transaction_id="018f7242-4387-7c98-a114-000000000098")
        exact((receipt.status,receipt.world_revision),("committed",1),"four-operation batch succeeds at grant ceiling")
        request=request_bytes(a,"POST","/v0/observe",encoded({"protocol_version":"0.1","world_id":"workshop"}))
        response,wire_bytes=exchange_sized(a,request)
        require(response is not None and response[0]==200,"exact 512-byte west observation succeeds")
        exact(wire_bytes,512,"actual transmitted JSON body at limit")
        handle=receipt.created[0].handle()
        next_transform=Transform(position_m=(-3.123456789012345,0.123456789012345,0.12345678901234))
        receipt=west.transact([SetTransform(handle,next_transform)],1,
                             transaction_id="018f7242-4387-7c98-a114-000000000097")
        exact(receipt.status,"committed","one-character-larger transform remains within write region")
        full,wire_bytes=exchange_sized(b,request_bytes(b,"POST","/v0/observe",encoded({"protocol_version":"0.1","world_id":"workshop"})))
        require(full is not None and full[0]==200,"larger whole-world read permitted for east")
        exact(wire_bytes,513,"actual transmitted over-limit JSON body size")
        denied(exchange(a,request),"/observation")
        status=wait_status(west,lambda x:x.usage.global_requests==0)
        exact((status.usage.working_bytes,status.next_sequence),(0,3),"observation boundary failure refunds without consuming sequence")
    evidence.retain_json("exact-limits.json",{"operations_accepted":4,"observation_accepted_bytes":512,
        "observation_denied_bytes":513})


def transport(executable,evidence):
    summary=[]
    with PolicySession(executable,max_runtime_ms=30000) as session:
        west,east=session.client("west"),session.client("east")
        a,b=desc(west),desc(east)
        evidence.register(a);evidence.register(b)
        body=encoded({"protocol_version":"0.1","world_id":"workshop"})
        for size,descriptor,accepted,path in ((3072,a,True,""),(3073,a,False,"/memory/working"),
                                               (16384,b,True,""),(16385,b,False,"/body")):
            request=request_bytes(descriptor,"POST","/v0/observe",body+b" "*(size-len(body)))
            start=time.monotonic()
            response=exchange(descriptor,request if accepted else headers_only(request))
            if accepted:
                require(response is not None and response[0]==200,"exact declared-byte boundary accepted")
                exact(response[1]["snapshot"]["world_revision"],0,"read does not mutate")
            else:
                denied(response,path)
                require(time.monotonic()-start<.75,"resource denial occurs before missing-body timeout")
            summary.append({"case":"body_boundary","bytes":size,"accepted":accepted})
        wait_status(west,lambda s:s.usage.global_requests==0)
        request=request_bytes(a,"POST","/v0/observe",body)
        slow=socket.create_connection(("127.0.0.1",a["port"]),timeout=3)
        slow.settimeout(3)
        try:
            slow.sendall(headers_only(request))
            active=wait_status(west,lambda s:s.usage.requests==1)
            exact(active.usage.working_bytes,73728+8*len(body),"incomplete body holds exact reservation")
            start=time.monotonic()
            denied(exchange(a,headers_only(request)),"/queue")
            require(time.monotonic()-start<.75,"excess A is promptly rejected")
            receipt=east.transact([CreateCube("B"),SetTransform(TemporaryTarget("B"),Transform(position_m=(3,0,0)))],0,
                                 transaction_id="018f7242-4387-7c98-a114-000000000099")
            exact((receipt.status,receipt.world_revision),("committed",1),"B commits through Owner beside incomplete A")
            require(west.policy_status().usage.requests==1,"A lease still alive during B progress")
            raw=bytearray()
            try:
                while True:
                    chunk=slow.recv(1024)
                    if not chunk: break
                    raw+=chunk
                    require(len(raw)<=4096,"timeout response remains bounded")
            except ConnectionResetError:
                pass
            exact(parse_response(bytes(raw)),None,"incomplete request closes without false success")
        finally:
            slow.close()
        status=wait_status(west,lambda s:s.usage.global_requests==0)
        exact((status.next_sequence,status.usage.working_bytes),(1,0),"timeout refunds and consumes no sequence")
        receipt=west.transact([CreateCube("A"),SetTransform(TemporaryTarget("A"),Transform(position_m=(-3,0,0)))],1,
                             transaction_id="018f7242-4387-7c98-a114-000000000100")
        exact((receipt.status,receipt.world_revision),("committed",2),"A recovers after timeout")
        summary.append({"case":"data_plane_isolation","excess_a":"rejected","b_revision":1,"a_timeout":"no_success","a_recovery_revision":2})
        before=canonical(east.observe().to_dict())
        for _ in range(10): denied(exchange(a,request),"/observation")
        status=wait_status(west,lambda s:s.usage.global_requests==0)
        exact((status.next_sequence,status.usage.working_bytes),(2,0),"observation rejection refunds without sequence consumption")
        exact(canonical(east.observe().to_dict()),before,"observation failures preserve world bytes")
        response=exchange(a,headers_only(request_bytes(a,"GET","/v0/policy",b"x")))
        require(response is not None and response[0]==400,"exempt routes refuse body without waiting")
        response=exchange(a,request_bytes(a,"POST","/v0/observe",encoded(
            {"protocol_version":"0.1","world_id":"workshop","principal":"east"})))
        require(response is not None and response[0]==400,"JSON principal cannot grant authority")
        old=a
        session.renew()
        west,east=session.client("west"),session.client("east")
        a,b=desc(west),desc(east);evidence.register(a);evidence.register(b)
        response=exchange(old,request_bytes(old,"GET","/v0/policy"))
        require(response is not None and response[0]==401,"renewal invalidates old credentials")
        for client in (west,east):
            status=client.policy_status()
            exact((status.usage.retained_bytes,status.next_sequence,status.world_revision),(384,1,2),
                  "renewal preserves principal memory and shared world")
    evidence.retain_json("transport.json",summary)


def main():
    parser=argparse.ArgumentParser()
    parser.add_argument("--executable",required=True,type=Path)
    parser.add_argument("--evidence-dir",type=Path)
    args=parser.parse_args()
    executable=args.executable.resolve(strict=True)
    evidence=Evidence(args.evidence_dir,executable)
    evidence.manifest.update(work_item="PR-007",example="policy.denied_edits")
    try:
        with tempfile.TemporaryDirectory(prefix="ow-policy-") as directory:
            output=Path(directory)/"example"
            result=evidence.run([sys.executable,str(ROOT/"sdk/python/examples/denied_edits.py"),
                "--executable",str(executable),"--output",str(output)],
                ["python","sdk/python/examples/denied_edits.py","--executable","<executable>","--output","<fresh-output>"],timeout=40)
            require(result.returncode==0,"public SDK example exits successfully")
            payload=(output/"policy.json").read_bytes()
            snapshots=validate(strict_json(payload))
            evidence.retain("policy.json",payload)
            for n,snapshot in enumerate(snapshots): evidence.retain(f"snapshot-{n}.owobj",canonical(snapshot))
        exact_limits(executable,evidence)
        transport(executable,evidence)
        evidence.finish("passed")
        print("policy.denied_edits passed",len(evidence.manifest["assertions"]),"assertions")
        return 0
    except Failure as error:
        evidence.finish("failed",str(error));raise
    except Exception:
        evidence.finish("failed","unexpected internal error; details withheld");raise


if __name__=="__main__": sys.exit(main_guard(main))
