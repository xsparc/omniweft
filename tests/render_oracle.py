#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Independent committed-snapshot and real GPU readback oracle; no renderer math imports."""
import argparse
import copy
import json
import math
import os
from pathlib import Path
import re
import struct
import sys
import tempfile

from render_test_support import Evidence, Failure, ROOT, digest, exact, execute_main, require, safe_relative

FIXTURE_PATH = ROOT / "tests/fixtures/render/world-cube.json"
FIXTURE = json.loads(FIXTURE_PATH.read_text(encoding="utf-8-sig"))
# Authoring/camera coordinates are JSON numbers; identity/revision fields remain strict integers.
for transform_value in FIXTURE["transforms"]:
    for field in ("position_m", "rotation_xyzw", "scale"):
        transform_value[field] = [float(value) for value in transform_value[field]]
FIXTURE["camera"]["position_m"] = [float(value) for value in FIXTURE["camera"]["position_m"]]
for field in ("left", "right", "bottom", "top", "near_m", "far_m"):
    FIXTURE["camera"][field] = float(FIXTURE["camera"][field])
COLORS = FIXTURE["face_rgba8"]
CLEAR = FIXTURE["clear_rgba8"]
CAMERA = FIXTURE["camera"]
UUID = FIXTURE["entity_uuid"]


def integer(value, label, minimum=0):
    require(type(value) is int and value >= minimum, label + ": integer")


def expected_object(revision):
    return {"object_id": 1, "entity_uuid": UUID, "generation": 1,
            "authoring_revision": revision, "transform": FIXTURE["transforms"][revision - 1]}


def expected_snapshot(revision):
    return {"format_version": 1, "world_id": "workshop", "seed": 7, "max_slots": 1024,
            "world_revision": revision, "slots": [{"entity_uuid": UUID, "generation": 1,
            "retired": False, "entity": {"prefab": "builtin.unit_cube",
            "authoring_revision": revision, "transform": FIXTURE["transforms"][revision - 1]}}]}


def expected_receipt(revision):
    return {"status": "committed", "durability": "volatile",
            "transaction_id": "018f7242-4387-7c98-a114-67787915a40" + str(revision),
            "world_revision": revision, "created": ([{"temporary_id": "cube", "world_id": "workshop",
            "entity_uuid": UUID, "generation": 1}] if revision == 1 else []), "errors": []}


def expected_pixel(x, y, width, height, revision):
    """Independent double ray/slab calculation, using rational yaw rather than native matrices."""
    px, py, pz = FIXTURE["transforms"][revision - 1]["position_m"]
    sx, sy, sz = FIXTURE["transforms"][revision - 1]["scale"]
    wx = -4 + 8 * (x + .5) / width
    wy = 3 - 6 * (y + .5) / height
    # Inverse of x'=.28*x+.96*z, z'=-.96*x+.28*z, then inverse scale.
    origin = ((.28 * (wx-px) - .96 * (5-pz)) / sx, (wy-py) / sy,
              (.96 * (wx-px) + .28 * (5-pz)) / sz)
    direction = (.96 / sx, 0, -.28 / sz)
    near, far, face = -math.inf, math.inf, -1
    for axis, (o, d) in enumerate(zip(origin, direction)):
        if d == 0:
            if not -.5 <= o <= .5:
                return 0, 1.0
            continue
        low, high = (-.5-o)/d, (.5-o)/d
        entry_face = axis*2 + (1 if d > 0 else 0)
        if low > high:
            low, high = high, low
        if low > near:
            near, face = low, entry_face
        far = min(far, high)
    if far < near or near < .1 or near > 20:
        return 0, 1.0
    return face + 1, (near-.1)/19.9


def expected_image(width, height, revision):
    labels, depths = [], []
    for y in range(height):
        for x in range(width):
            label, depth = expected_pixel(x, y, width, height, revision)
            labels.append(label)
            depths.append(depth)
    band = [False] * (width*height)
    # Exclusions come only from the expected labels, never captured pixels.
    for y in range(height):
        for x in range(width):
            i = y*width+x
            band[i] = any(labels[ny*width+nx] != labels[i]
                          for ny in range(max(0,y-1), min(height,y+2))
                          for nx in range(max(0,x-1), min(width,x+2)))
    return labels, depths, band


def validate_pixels(color, ids, depth, width, height, revision, color_format, phase):
    pixels = width*height
    require(len(color) == len(ids) == len(depth) == pixels*4, phase + ": GPU attachment byte lengths")
    native_ids = [v[0] for v in struct.iter_unpack("<I", ids)]
    native_depth = [v[0] for v in struct.iter_unpack("<f", depth)]
    require(all(value in (0,1) for value in native_ids), phase + ": GPU IDs are known everywhere")
    require(all(math.isfinite(value) and 0 <= value <= 1 for value in native_depth),
            phase + ": GPU depths are finite and normalized everywhere")
    labels, expected_depths, band = expected_image(width,height,revision)
    core = [i for i in range(pixels) if not band[i]]
    require(any(labels[i] for i in core) and any(not labels[i] for i in core),
            phase + ": expected foreground and untouched background are nonempty")
    require(all(native_ids[i] == int(labels[i] != 0) for i in core), phase + ": GPU ID mask")
    swizzle = (2,1,0,3) if color_format == "B8G8R8A8_UNORM" else (0,1,2,3)
    require(all(abs(color[i*4+swizzle[c]] - (COLORS[labels[i]-1] if labels[i] else CLEAR)[c]) <= 1
                for i in core for c in range(4)), phase + ": GPU face and background color")
    require(all(abs(native_depth[i]-expected_depths[i]) <= 1e-5 for i in core),
            phase + ": GPU normalized depth")
    # RGBA8 is bounded by representation; ID and depth checks also cover the excluded band.
    return {"width": width, "height": height, "tested_pixels": len(core),
            "excluded_expected_edge_pixels": sum(band),
            "expected_foreground_pixels": sum(label != 0 for label in labels)}


def oracle_math_selfcheck():
    # Independent literal bounds: rev1 projects x95.2..144.8,y100..140.
    require(expected_pixel(94,120,320,240,1)[0] == 0, "ray oracle left exterior literal")
    require(expected_pixel(96,120,320,240,1)[0] != 0, "ray oracle left interior literal")
    require(expected_pixel(144,120,320,240,1)[0] != 0, "ray oracle right interior literal")
    require(expected_pixel(145,120,320,240,1)[0] == 0, "ray oracle right exterior literal")
    require(expected_pixel(120,99,320,240,1)[0] == 0, "ray oracle upper exterior literal")
    require(expected_pixel(120,100,320,240,1)[0] != 0, "ray oracle upper interior literal")
    face, center_depth = expected_pixel(119.5,119.5,320,240,1)
    require(face == 2 and abs(center_depth-1051/4776) < 1e-14, "ray oracle rational center depth and face")
    # Rev2 scaled projected x178..222, y80..140.
    require(expected_pixel(177,110,320,240,2)[0] == 0, "ray oracle transformed left exterior")
    require(expected_pixel(178,110,320,240,2)[0] != 0, "ray oracle transformed left interior")
    require(expected_pixel(221,110,320,240,2)[0] != 0, "ray oracle transformed right interior")
    require(expected_pixel(222,110,320,240,2)[0] == 0, "ray oracle transformed right exterior")
    require(expected_pixel(200,79,320,240,2)[0] == 0, "ray oracle transformed upper exterior")
    require(expected_pixel(200,80,320,240,2)[0] != 0, "ray oracle transformed upper interior")


def inspect_packet(packet, revision, geometry=True):
    expected_keys = {"phase","world_id","world_revision","camera","objects","vertices","indices"}
    require(type(packet) is dict and packet.keys() == expected_keys, "packet: fields")
    exact(packet["phase"], "initial" if revision == 1 else "transformed", "packet phase")
    exact(packet["world_id"], "workshop", "packet world")
    exact(packet["world_revision"], revision, "packet revision")
    exact(packet["camera"], CAMERA, "packet camera")
    exact(packet["objects"], [expected_object(revision)], "packet objects")
    if not geometry:
        return
    vertices, indices = packet["vertices"], packet["indices"]
    require(type(vertices) is list and len(vertices) == 24 and type(indices) is list and len(indices) == 36,
            "packet: indexed cube counts")
    found = [[] for unused in range(6)]
    sx,sy,sz = FIXTURE["transforms"][revision-1]["scale"]
    px,py,pz = FIXTURE["transforms"][revision-1]["position_m"]
    expected = []
    for face in range(6):
        corners = []
        for a in (-.5,.5):
            for b in (-.5,.5):
                local = [0,0,0]
                local[face//2] = .5 if face%2 == 0 else -.5
                local[(face//2+1)%3],local[(face//2+2)%3] = a,b
                x,y,z = local[0]*sx,local[1]*sy,local[2]*sz
                corners.append([(.28*x+.96*z+px)/4, -(y+py)/3, (5-(-.96*x+.28*z+pz)-.1)/19.9, 1])
        expected.append(corners)
    for index, vertex in enumerate(vertices):
        require(type(vertex) is dict and vertex.keys() == {"clip_position","color","object_id"}, "vertex: fields")
        exact(vertex["object_id"],1,"vertex object ID")
        require(type(vertex["color"]) is list and len(vertex["color"]) == 4, "vertex color components")
        face_matches = [f for f in range(6) if all(type(v) in (int,float) and math.isfinite(v) and abs(v-COLORS[f][c]/255) <= 2e-6
                                                     for c,v in enumerate(vertex["color"]))]
        require(len(face_matches) == 1,"literal vertex face color")
        face = face_matches[0]
        clip = vertex["clip_position"]
        require(type(clip) is list and len(clip) == 4 and all(type(v) in (int,float) and math.isfinite(v) for v in clip),
                "finite clip vertex")
        matches = [i for i, corner in enumerate(expected[face]) if all(abs(clip[c]-corner[c]) <= 2e-6 for c in range(4))]
        require(len(matches) == 1,"independent projected corner")
        expected[face].pop(matches[0])
        found[face].append(index)
    require(all(not corners for corners in expected),"all distinct projected corners present")
    require(all(type(i) is int and 0 <= i < 24 for i in indices),"integer index bounds")
    for face, members in enumerate(found):
        triangles = [indices[i:i+3] for i in range(0,36,3) if indices[i] in members]
        require(len(triangles) == 2 and all(len(set(t)) == 3 and set(t) <= set(members) for t in triangles),
                "two nondegenerate triangles on each face")
        shared = set(triangles[0]) & set(triangles[1])
        require(len(shared) == 2,"face triangles share one diagonal")
        require(all(abs(2*sum(vertices[i]["clip_position"][c] for i in shared) -
                            sum(vertices[i]["clip_position"][c] for i in members)) <= 8e-6 for c in range(3)),
                "triangle shared edge is the quad diagonal")


def format_name(value):
    require(type(value) is str, "attachment format is a string")
    return value.removeprefix("VK_FORMAT_")


def attachment_bytes(output, descriptor, width, height, expected_formats, label, evidence):
    require(type(descriptor) is dict and descriptor.keys() == {"path","format","origin","row_stride_bytes"},
            label + ": descriptor fields")
    relative = safe_relative(descriptor["path"])
    name = format_name(descriptor["format"])
    require(name in expected_formats, label + ": exact format")
    exact(descriptor["origin"],"top_left",label + ": origin")
    exact(descriptor["row_stride_bytes"],width*4,label + ": row stride")
    target = output / relative
    require(target.is_file() and not target.is_symlink() and target.resolve().is_relative_to(output.resolve()),
            label + ": artifact belongs to this run")
    data = target.read_bytes()
    require(len(data) == width*height*4,label + ": exact length")
    return data,name


def inspect_frame(frame, revision, phase, output, evidence):
    keys = {"phase","frame_id","world_id","world_revision","objects","camera","pixel_extent",
            "source_generation","swapchain_generation","image_index","submission_serial",
            "present_result","attachments","copy"}
    require(type(frame) is dict and frame.keys() == keys, phase + ": frame fields")
    exact(frame["phase"],phase,phase + ": name")
    for key in ("frame_id","source_generation","swapchain_generation","submission_serial"):
        integer(frame[key],phase+"/"+key,1)
    integer(frame["image_index"],phase+"/image_index")
    require(frame["image_index"] < 3,phase + ": swapchain image bound")
    exact(frame["world_id"],"workshop",phase + ": world")
    exact(frame["world_revision"],revision,phase + ": committed world revision")
    exact(frame["objects"],[expected_object(revision)],phase + ": committed object")
    exact(frame["camera"],CAMERA,phase + ": camera")
    extent = frame["pixel_extent"]
    require(type(extent) is dict and extent.keys() == {"width","height"},phase + ": extent fields")
    width,height = extent["width"],extent["height"]
    integer(width,phase+"/width",1); integer(height,phase+"/height",1)
    require(width <= 1024 and height <= 1024 and width*height <= 786432,phase + ": allocation extent bounds")
    require(frame["present_result"] in ("VK_SUCCESS","VK_SUBOPTIMAL_KHR"),phase + ": actual successful queued present")
    attachments = frame["attachments"]
    require(type(attachments) is dict and attachments.keys() == {"color","object_id","depth"},phase + ": attachments")
    color,color_format = attachment_bytes(output,attachments["color"],width,height,{"R8G8B8A8_UNORM","B8G8R8A8_UNORM"},phase+" color",evidence)
    ids,unused = attachment_bytes(output,attachments["object_id"],width,height,{"R32_UINT"},phase+" ID",evidence)
    depth,unused = attachment_bytes(output,attachments["depth"],width,height,{"D32_SFLOAT"},phase+" depth",evidence)
    expected_copy = {"command":"vkCmdCopyImage","source_generation":frame["source_generation"],
                     "swapchain_generation":frame["swapchain_generation"],"image_index":frame["image_index"],
                     "pixel_extent":extent,"source_format":attachments["color"]["format"],
                     "destination_format":attachments["color"]["format"],"full_extent":True}
    exact(frame["copy"],expected_copy,phase + ": presented color copy provenance")
    stats = validate_pixels(color,ids,depth,width,height,revision,color_format,phase)
    public_frame = copy.deepcopy(frame)
    for role,data in (("color",color),("object_id",ids),("depth",depth)):
        public_name = "readbacks/"+phase+"-"+role+".bin"
        evidence.retain(public_name,data)
        public_frame["attachments"][role]["path"] = ((evidence.retention_prefix+"/") if evidence.retention_prefix else "")+public_name
    # Published artifact names are fixed by phase/role, never borrowed from native filenames.
    evidence.retain_json("frames/"+phase+".json",{"frame":public_frame,"pixel_assertions":stats})
    return stats


def inspect_lifecycle(report):
    events = report["window_events"]
    require(type(events) is list, "actual window events list")
    for event in events:
        require(type(event) is dict and event.keys() == {"kind","pixel_extent","minimized","submission_serial","swapchain_generation"},
                "window event fields")
        require(event["kind"] in ("resized","minimized","restored"),"window event kind")
        require(type(event["minimized"]) is bool,"window state is boolean")
        for key in ("submission_serial","swapchain_generation"):
            integer(event[key],"window event/"+key)
        extent = event["pixel_extent"]
        require(type(extent) is dict and extent.keys() == {"width","height"},"event extent fields")
        integer(extent["width"],"event width"); integer(extent["height"],"event height")
    frames = report["frames"]
    # Initial/duplicate OS size events are legitimate; select the actual changed-extent interval.
    changed = [(i,e) for i,e in enumerate(events) if e["kind"] == "resized" and
               e["pixel_extent"] == frames[2]["pixel_extent"] and
               frames[1]["submission_serial"] <= e["submission_serial"] < frames[2]["submission_serial"]]
    require(bool(changed),"actual changed-extent event occurs between transformed and resized captures")
    resize_index,resize_event = changed[0]
    minimizes = [(i,e) for i,e in enumerate(events) if i > resize_index and e["kind"] == "minimized"]
    require(bool(minimizes),"actual minimized event follows resize")
    minimize_index,minimize_event = minimizes[0]
    restores = [e for i,e in enumerate(events) if i > minimize_index and e["kind"] == "restored"]
    require(bool(restores),"actual restored event follows minimize")
    restore_event = restores[0]
    require(not resize_event["minimized"] and minimize_event["minimized"] and not restore_event["minimized"],
            "actual minimized state and recovery observed")
    require(minimize_event["submission_serial"] == restore_event["submission_serial"],
            "no GPU submission occurs while minimized")
    require(frames[2]["pixel_extent"] != frames[1]["pixel_extent"] and
            frames[2]["swapchain_generation"] > frames[1]["swapchain_generation"],
            "resize changes actual extent and swapchain generation")
    require(resize_event["pixel_extent"] == frames[2]["pixel_extent"],"resize event matches new framebuffer")
    require(minimize_event["submission_serial"] >= frames[2]["submission_serial"] and
            frames[3]["submission_serial"] > restore_event["submission_serial"],
            "restored capture follows minimized event interval")
    serials = [frame["submission_serial"] for frame in frames]
    require(serials == sorted(set(serials)),"captured submissions strictly advance")
    frame_ids = [frame["frame_id"] for frame in frames]
    require(frame_ids == sorted(set(frame_ids)),"captured frame IDs strictly advance")
    trace = report["lifecycle_events"]
    require(type(trace) is list and len(trace) <= 120*8+16,"bounded actual Vulkan lifecycle trace")
    submitted,queued,graphics_done,present_done = {},set(),set(),set()
    retired,queued_resize = set(),False
    permitted = {"submit","present_queued","graphics_complete","present_complete","retired",
                 "resize_requested","minimize_requested","restore_requested"}
    for entry in trace:
        require(type(entry) is dict and entry.keys() == {"event","submission_serial","swapchain_generation","graphics_pending","present_pending"},
                "lifecycle event fields")
        event,serial,generation = entry["event"],entry["submission_serial"],entry["swapchain_generation"]
        require(event in permitted,"lifecycle event is a known Vulkan/host transition")
        integer(serial,"lifecycle submission serial")
        integer(generation,"lifecycle swapchain generation",1)
        require(type(entry["graphics_pending"]) is bool and type(entry["present_pending"]) is bool,
                "lifecycle pending flags are boolean")
        if event == "submit":
            require(serial > 0 and serial not in submitted and generation not in retired,"submission owns a live generation once")
            submitted[serial] = generation
        elif event == "present_queued":
            require(serial in submitted and serial not in queued and submitted[serial] == generation,"presentation follows the matching submission")
            queued.add(serial)
        elif event == "graphics_complete":
            require(serial in submitted and submitted[serial] == generation and serial not in graphics_done,"graphics completion follows one actual submission")
            graphics_done.add(serial)
        elif event == "present_complete":
            require(serial in queued and submitted[serial] == generation and serial not in present_done,"presentation completion follows a queued presentation")
            present_done.add(serial)
        elif event == "retired":
            require(generation not in retired,"resource generation retires once")
            owned = {s for s,g in submitted.items() if g == generation}
            require(owned <= graphics_done and owned <= present_done,
                    "resource retirement follows graphics and present fence completion")
            require(not entry["graphics_pending"] and not entry["present_pending"],"retired resource has no pending GPU owner")
            retired.add(generation)
        elif event == "resize_requested":
            queued_resize |= (frames[1]["submission_serial"] <= serial < frames[2]["submission_serial"] and
                              generation == frames[1]["swapchain_generation"] and
                              (entry["graphics_pending"] or entry["present_pending"]))
        if event != "retired":
            require(entry["graphics_pending"] == (serial in submitted and serial not in graphics_done),
                    "graphics pending trace agrees with observed completion")
            require(entry["present_pending"] == (serial in queued and serial not in present_done),
                    "presentation pending trace agrees with observed completion")
    for frame in frames:
        require(frame["submission_serial"] in submitted and
                frame["swapchain_generation"] == submitted[frame["submission_serial"]],
                "captured frame generation matches its actual submission")
    require(queued_resize,"resize request actually exercises queued work")
    require(set(serials) <= queued and set(submitted) == graphics_done == present_done,
            "every captured and outstanding submission finishes before shutdown")
    require(set(submitted.values()) <= retired,"shutdown retires every used resource generation")



def inspect_report(report, mode, output, evidence, case_set):
    require(type(report) is dict,"native report is an object")
    exact(report["schema_version"],1,"report schema")
    exact(report["example"],"render.world_cube","report example")
    exact(report["seed"],7,"report seed")
    exact(report["mode"],mode,"report mode")
    exact(report["status"],"passed","native bounded run completed")
    exact(report["errors"],[],"native errors")
    exact(report["receipts"],[expected_receipt(1),expected_receipt(2)],"actual typed receipts")
    exact(report["snapshots"],[expected_snapshot(1),expected_snapshot(2)],"complete committed snapshots")
    require(type(report["packets"]) is list and len(report["packets"]) == 2,"two detached packets")
    if mode == "gpu":
        device = report["device"]
        require(type(device) is dict and device.keys() == {"name","api_version","driver_version","device_type"},"device allowlist")
        require(device["device_type"] in ("discrete","integrated","discrete_gpu","integrated_gpu","DISCRETE_GPU","INTEGRATED_GPU",
                                         "VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU","VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU"),
                "real hardware GPU type is required")
        for field in ("name","api_version","driver_version"):
            value = device[field]
            require(type(value) is str and 0 < len(value) <= 128 and
                    re.fullmatch(r"[A-Za-z0-9 .()+,_-]+",value) is not None,
                    "generic device metadata contains no paths or opaque identifiers")
        evidence.manifest["environment"]["gpu"] = device
        validation = report["validation"]
        exact(validation,{"enabled":True,"errors":0,"warnings":0,"messages":[]},"core and synchronization validation")
        require(type(report["frames"]) is list and len(report["frames"]) == 4,"four required real GPU capture phases")
        phases = ("initial","transformed","resized","restored")
        for index,phase in enumerate(phases):
            if case_set == "transform" and index >= 2:
                break
            inspect_frame(report["frames"][index],1 if index == 0 else 2,phase,output,evidence)
        if case_set == "all":
            inspect_lifecycle(report)
            evidence.retain_json("window-events.json",report["window_events"])
            evidence.retain_json("lifecycle-events.json",report["lifecycle_events"])
        # Pixel evidence precedes CPU geometry checks, so the real transform mutant
        # is rejected by actual GPU output rather than a duplicate native assertion.


    else:
        exact(report["gpu_checks"],"not_run","headless never passes GPU")
        exact(report["frames"],[],"headless has no captured GPU frames")

    for index,packet in enumerate(report["packets"]):
        inspect_packet(packet,index+1)
    evidence.retain_json("committed-inputs.json",{"receipts":report["receipts"],"snapshots":report["snapshots"]})
    evidence.retain_json("packets.json",report["packets"])


def run_example(executable, output, mode, evidence, env=None):
    command = [str(executable),"--example","render.world_cube","--"+mode,"--seed","7","--verify","--output",str(output)]
    public = ["<executable>","--example","render.world_cube","--"+mode,"--seed","7","--verify","--output","<run-output>"]
    result = evidence.run(command,public,env=env,timeout=45)
    return result


def strict_json_object(pairs):
    value = {}
    for key,item in pairs:
        if key in value:
            raise Failure("native result contains a duplicate JSON key")
        value[key] = item
    return value


def finite_json_float(token):
    value = float(token)
    if not math.isfinite(value):
        raise Failure("native result contains a nonfinite JSON number")
    return value


def reject_json_constant(unused):
    raise Failure("native result contains a nonfinite JSON number")


def read_report(output):
    target = output / "result.json"
    require(target.is_file(),"native result exists")
    try:
        return json.loads(target.read_text(encoding="utf-8"),object_pairs_hook=strict_json_object,parse_constant=reject_json_constant,parse_float=finite_json_float)
    except (ValueError,UnicodeError):
        raise Failure("native result is valid UTF-8 JSON") from None


def startup_negative(executable, temporary, evidence):
    # Normal loader configuration is left untouched; filter applies to this child only.
    environment = os.environ.copy()
    environment["VK_LOADER_DRIVERS_DISABLE"] = "*"
    environment.pop("VK_LOADER_DRIVERS_SELECT",None)
    output = temporary / "no-driver"
    result = run_example(executable,output,"gpu",evidence,environment)
    evidence.manifest["commands"][-1]["environment_overrides"] = {"VK_LOADER_DRIVERS_DISABLE":"*", "VK_LOADER_DRIVERS_SELECT":"unset"}
    require(result.returncode == 3,"unavailable driver startup returns controlled unsupported status")
    report = read_report(output)
    exact(report["status"],"unsupported","missing-driver report status")
    exact(report["frames"],[],"missing driver cannot claim a presented frame")
    require(type(report["errors"]) is list and len(report["errors"]) >= 1,"unsupported startup has a structured reason")
    codes = []
    for error in report["errors"]:
        require(type(error) is dict and type(error.get("code")) is str and
                re.fullmatch("[A-Z][A-Z0-9_]*",error["code"]) is not None,"unsupported reason is a safe code")
        codes.append(error["code"])
    evidence.retention_prefix = "startup-negative"
    evidence.retain_json("unsupported-startup.json",{"status":"unsupported","exit_code":result.returncode,"error_codes":codes})
    recovery_output = temporary / "startup-recovery"
    recovery = run_example(executable,recovery_output,"gpu",evidence)
    require(recovery.returncode == 0,"normal startup recovers after child-only driver filter")
    # Recheck recovery through real pixels with original thresholds.
    evidence.retention_prefix = "startup-recovery"
    recovery_report = read_report(recovery_output)
    inspect_report(recovery_report,"gpu",recovery_output,evidence,"transform")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable",type=Path,required=True)
    modes = parser.add_mutually_exclusive_group()
    modes.add_argument("--mode",choices=("headless","gpu"))
    modes.add_argument("--headless",action="store_true")
    modes.add_argument("--gpu",action="store_true")
    parser.add_argument("--case-set",choices=("all","transform"),default="all")
    parser.add_argument("--evidence-dir",type=Path)
    args = parser.parse_args()
    mode = args.mode or ("gpu" if args.gpu else "headless")
    require(not (args.evidence_dir and args.case_set != "all"),
            "retained evidence requires the complete lifecycle and startup case set")
    executable = args.executable.resolve(strict=True)
    evidence = Evidence(args.evidence_dir,executable,mode)
    evidence.manifest["case_set"] = args.case_set
    try:
        oracle_math_selfcheck()
        evidence.retain_json("fixture.json",FIXTURE)
        with tempfile.TemporaryDirectory(prefix="ow-render-test-") as directory:
            temporary = Path(directory)
            output = temporary / "normal"
            result = run_example(executable,output,mode,evidence)
            require(result.returncode == 0,"public render example succeeds on the required lane")
            evidence.retention_prefix = "normal"
            inspect_report(read_report(output),mode,output,evidence,args.case_set)
            if mode == "gpu" and args.case_set == "all":
                startup_negative(executable,temporary,evidence)
        evidence.manifest["lanes"] = {"cpu":"passed","gpu":"passed" if mode == "gpu" else "not_run"}
        evidence.finish("passed")
        print(json.dumps({"status":"passed","mode":mode,"candidate_sha":evidence.manifest["candidate_sha"],
                          "assertions":len(evidence.manifest["assertions"])}))
        return 0
    except Failure as error:
        evidence.manifest["lanes"] = {"cpu":"failed" if mode == "headless" else "not_run", "gpu":"failed" if mode == "gpu" else "not_run"}
        evidence.finish("failed",str(error))
        raise
    except Exception:
        evidence.finish("failed","unexpected internal error; private details withheld")
        raise


if __name__ == "__main__":
    raise SystemExit(execute_main(main))

