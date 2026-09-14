#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Independent real provider, fixed-step liveness, authoring and live GPU oracle."""
import argparse
import copy
import json
import os
from pathlib import Path
import socket
import subprocess
import sys
import tempfile
import time
from unittest import mock

from agents_test_support import (FIXTURE, ROOT, Evidence, Failure, canonical, compare, digest,
    expected_state, inspect_runtime, inspect_state, main_guard, receipt, require, strict_json,
    validate_fixture)
from agents_gpu_oracle import inspect_gpu
from render_test_support import timestamp


class Processes:
    """Observe real production subprocess launches; record no private descriptors/PIDs."""
    def __init__(self, evidence, executable, sdk_root):
        self.evidence, self.executable, self.sdk_root = evidence, executable, sdk_root
        self.processes = []
        self.original = subprocess.Popen

    def launch(self, args, *positional, **kwargs):
        require(type(args) is list and all(type(v) is str for v in args), "production child uses explicit argument vector")
        if Path(args[0]).resolve() == self.executable:
            public = ["<agents-executable>"]
            require(len(args) % 2 == 1, "native arguments are bounded pairs")
            for index in range(1, len(args), 2):
                key, value = args[index:index+2]
                require(key in ("--world","--seed","--max-slots","--session-ttl-ms","--max-runtime-ms",
                                "--max-requests","--gpu","--interactive","--output"), "native child flag is allowlisted")
                if key == "--output":
                    shown = "<fresh-native-output>"
                else:
                    require(value == "workshop" if key == "--world" else value.isdecimal(),
                            "native public argument value is fixed or numeric")
                    shown = value
                public.extend((key, shown))
            role = "native"
        else:
            require(len(args) == 2 and Path(args[0]).resolve() == Path(sys.executable).resolve()
                    and Path(args[1]).resolve() == self.sdk_root / "omniweft_sdk/_scripted_worker.py",
                    "provider is the exact separate repository worker")
            public = ["<python>", "sdk/python/omniweft_sdk/_scripted_worker.py"]
            role = "provider"
        record = {"command": public, "started_at": timestamp(), "exit_code": None}
        self.evidence.manifest["commands"].append(record)
        process = self.original(args, *positional, **kwargs)
        self.processes.append((role, process, record))
        return process

    def __enter__(self):
        self.patch = mock.patch("subprocess.Popen", self.launch)
        self.patch.__enter__()
        return self

    def __exit__(self, kind, unused, traceback):
        self.patch.__exit__(kind, unused, traceback)
        for role, process, record in self.processes:
            record["exit_code"] = process.poll()
            record["finished_at"] = timestamp()
        if kind is None:
            require([r for r,p,c in self.processes] == ["native","provider"], "one real native and one real provider process ran")
            pids = [p.pid for r,p,c in self.processes]
            require(len(set(pids + [os.getpid()])) == 3, "provider, owner host and observer are separate OS processes")
            require(all(c["exit_code"] == 0 for r,p,c in self.processes), "native and provider both stop successfully")


def observation(client, revision, gpu, evidence, label, previous=None):
    value = client.runtime().to_dict()
    inspect_runtime(value, revision, gpu, previous)
    evidence.retain_json("observations/"+label+".json", value)
    return value


def held_progress(client, revision, gpu, evidence, phase):
    first = observation(client, revision, gpu, evidence, phase+"-0")
    current = first
    ticks = {first["simulation_tick"]}
    deadline = time.monotonic()+3.0
    index = 0
    while time.monotonic() < deadline:
        time.sleep(.025)
        index += 1
        current = observation(client, revision, gpu, evidence, phase+"-"+str(index), current)
        ticks.add(current["simulation_tick"])
        if current["simulation_tick"] >= first["simulation_tick"]+6 and len(ticks) >= 3:
            break
    require(current["simulation_tick"] >= first["simulation_tick"]+6 and len(ticks) >= 3,
            phase+": held provider cannot block real tick progress")
    require(canonical(first["snapshot"]) == canonical(current["snapshot"]),
            phase+": tick progress leaves complete authoring bytes unchanged")
    return current


def wait_presented(client, revision, evidence, label):
    deadline = time.monotonic()+8.0
    prior = None
    while time.monotonic() < deadline:
        value = client.runtime().to_dict()
        inspect_runtime(value, revision, True, prior)
        prior = value
        if value["presentation"]["ready"] and value["presentation"]["world_revision"] == revision:
            evidence.retain_json("observations/"+label+".json", value)
            return value
        time.sleep(.025)
    raise Failure("actual GPU presents the required committed revision before provider release")


def incomplete_input(host, observer, gpu, evidence):
    before = observation(observer, 0, gpu, evidence, "incomplete-before")
    # The descriptor is used privately only to reach the actual production socket.
    descriptor = strict_json(host._provider_descriptor())
    evidence.register(descriptor)
    with socket.create_connection(("127.0.0.1", descriptor["port"]), timeout=2) as connection:
        connection.sendall(b"GET /v0/runtime HTTP/1.1\r\n")
        time.sleep(.4)
    after = observation(observer, 0, gpu, evidence, "incomplete-after", before)
    require(after["simulation_tick"] >= before["simulation_tick"]+12,
            "incomplete network input cannot defer all ticks until capped catch-up")
    require(canonical(before["snapshot"]) == canonical(after["snapshot"]),
            "incomplete network input leaves complete authoring state unchanged")


def inspect_progress(value, event, revisions, evidence):
    compare(value.event, event, "provider exact progress barrier")
    require(len(value.receipts) == len(revisions) and len(value.snapshots) == max(1,len(revisions)),
            "provider bounded progress counts")
    expected_revisions = revisions if revisions else [0]
    for item, revision in zip(value.snapshots, expected_revisions):
        state = item.to_dict()
        raw = inspect_state(state, revision, "provider observed state")
        evidence.retain_json("snapshots/revision-"+str(revision)+".json", state)
        evidence.retain("canonical/revision-"+str(revision)+".bin", raw)
    for item, revision in zip(value.receipts, revisions):
        actual = item.to_dict()
        compare(actual, receipt(revision), "provider independently expected receipt")
        evidence.retain_json("receipts/revision-"+str(revision)+".json", actual)


def inspect_native(output, gpu, evidence):
    path = output / "result.json"
    require(path.is_file() and not path.is_symlink() and path.stat().st_size <= 4194304,
            "native run publishes a bounded report")
    report = strict_json(path.read_bytes())
    require(type(report) is dict and report.keys() ==
            {"schema_version","example","status","runtime","device","validation","frames",
             "window_events","lifecycle_events","errors"}, "native agent report exact fields")
    compare(report["schema_version"],1,"native report schema")
    compare(report["example"],"agents.mock_builder","native report example")
    compare(report["status"],"passed","native completed run")
    compare(report["errors"],[],"native completed without errors")
    inspect_runtime(report["runtime"],3,gpu)
    if gpu:
        inspect_gpu(report, output, evidence)
    else:
        compare(report["device"],{},"headless device metadata")
        compare(report["validation"],{"enabled":False,"errors":0,"warnings":0,"messages":[]},"headless validation is not GPU proof")
        for field in ("frames","window_events","lifecycle_events"):
            compare(report[field],[],"headless has no GPU "+field)
    # Readback descriptor paths have separate fixed-name retention in inspect_gpu.
    evidence.retain_json("native-runtime.json",report["runtime"])
    return digest(canonical(report["runtime"]["snapshot"]))


def live_scene(sdk, executable, sdk_root, output, gpu, evidence):
    with Processes(evidence, executable, sdk_root):
        with sdk.NativeSession(executable,max_slots=8,gpu=gpu,output=output) as host:
            observer = host.client()
            incomplete_input(host, observer, gpu, evidence)
            with sdk.ScriptedBuilder(host) as provider:
                inspect_progress(provider.initial,"ready",[],evidence)
                ready = held_progress(observer,0,gpu,evidence,"ready")
                first = provider.create_first()
                inspect_progress(first,"first",[1],evidence)
                committed = observation(observer,1,gpu,evidence,"first-committed",ready)
                require(committed["simulation_tick"] > ready["simulation_tick"],
                        "first SDK receipt follows an actual later tick publication")
                if gpu:
                    wait_presented(observer,1,evidence,"first-presented-before-release")
                before_finish = held_progress(observer,1,gpu,evidence,"first")
                completed = provider.finish()
                inspect_progress(completed,"done",[2,3],evidence)
                final = observation(observer,3,gpu,evidence,"finished",before_finish)
                require(final["simulation_tick"] >= before_finish["simulation_tick"]+2,
                        "two sequential SDK receipts follow two completed tick boundaries")
                if gpu:
                    final = wait_presented(observer,3,evidence,"final-presented")
                require(final["snapshot"]["world_revision"] == 3,"same provider recovers and finishes fixed arrangement")
        require(host.returncode == 0,"native context clean shutdown after live builder")
    return inspect_native(output,gpu,evidence)


def public_example(executable,sdk_root,output,gpu,evidence):
    mode="--gpu" if gpu else "--headless"
    command=[sys.executable,str(sdk_root/"examples/mock_builder.py"),"--executable",str(executable),
             "--output",str(output),"--seed","7",mode,"--verify"]
    public=["<python>","sdk/python/examples/mock_builder.py","--executable","<agents-executable>",
            "--output","<fresh-example-output>","--seed","7",mode,"--verify"]
    result=evidence.run(command,public,timeout=45)
    require(result.returncode==0,"actual public mock-builder example exits successfully")
    path=output/"result.json"
    require(path.is_file() and not path.is_symlink() and path.stat().st_size<=4194304,"public example bounded report")
    report=strict_json(path.read_bytes())
    require(type(report) is dict and report.keys()=={"schema_version","example","seed","status","lane","receipts",
                                                   "snapshots","runtime_observations"},"public example exact fields")
    for key,value in (("schema_version",1),("example","agents.mock_builder"),("seed",7),
                      ("status","passed"),("lane","gpu" if gpu else "cpu")):
        compare(report[key],value,"public example "+key)
    compare(report["receipts"],[receipt(r) for r in (1,2,3)],"public example literal receipts")
    require(type(report["snapshots"]) is list and len(report["snapshots"])==4,"public example four actual states")
    for revision,state in enumerate(report["snapshots"]):
        raw=inspect_state(state,revision,"public example state")
        evidence.retain("canonical/revision-"+str(revision)+".bin",raw)
    observations=report["runtime_observations"]
    require(type(observations) is list and len(observations)==5,"public example five runtime observations")
    for index,revision in enumerate((0,0,1,1,3)):
        inspect_runtime(observations[index],revision,gpu,observations[index-1] if index else None)
    for a,b in ((0,1),(2,3)):
        require(observations[b]["simulation_tick"] >= observations[a]["simulation_tick"]+6,
                "public example actual provider delay permits real tick progress")
        require(canonical(observations[a]["snapshot"])==canonical(observations[b]["snapshot"]),
                "public example paused provider preserves authoring bytes")
    if gpu:
        require(observations[2]["presentation"]["world_revision"]==1 and
                observations[4]["presentation"]["world_revision"]==3,"public example presents live first and final revisions")
    evidence.retain_json("sdk-example.json",report)
    return inspect_native(output/"native",gpu,evidence)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable",type=Path,required=True)
    parser.add_argument("--sdk-root",type=Path,default=ROOT/"sdk/python")
    mode=parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--headless",action="store_true")
    mode.add_argument("--gpu",action="store_true")
    parser.add_argument("--evidence-dir",type=Path)
    args=parser.parse_args()
    executable=args.executable.resolve(strict=True)
    sdk_root=args.sdk_root.resolve(strict=True)
    require(not args.evidence_dir or sdk_root==(ROOT/"sdk/python").resolve(),
            "retained agent SDK belongs to the exact candidate checkout")
    sys.path.insert(0,str(sdk_root))
    import omniweft_sdk as sdk
    require(Path(sdk.__file__).resolve().is_relative_to(sdk_root),"actual agent SDK imports from declared source")
    evidence=Evidence(args.evidence_dir,executable,"gpu" if args.gpu else "cpu")
    try:
        validate_fixture()
        evidence.retain_json("fixture.json",FIXTURE)
        with tempfile.TemporaryDirectory(prefix="ow-agents-oracle-") as temporary:
            base=Path(temporary)
            evidence.retention_prefix="independent-live"
            first=live_scene(sdk,executable,sdk_root,base/"live",args.gpu,evidence)
            evidence.retention_prefix="public-example"
            repeated=public_example(executable,sdk_root,base/"example",args.gpu,evidence)
            require(first==repeated==FIXTURE["canonical_states"][3]["sha256"],
                    "independent and public runs produce the same preregistered authoring hash")
        evidence.retention_prefix=""
        evidence.retain_json("determinism.json",{"seed":7,"independent_authoring_sha256":first,
                                               "public_example_authoring_sha256":repeated})
        evidence.finish("passed")
        print(json.dumps({"status":"passed","example":"agents.mock_builder",
                          "assertions":len(evidence.manifest["assertions"])}))
        return 0
    except Failure as error:
        evidence.finish("failed",str(error))
        raise
    except Exception:
        evidence.finish("failed","unexpected internal error; private details withheld")
        raise


if __name__=="__main__":
    raise SystemExit(main_guard(main))
