# SPDX-License-Identifier: Apache-2.0
"""Independent multi-cube pixel rays and actual presentation/fence provenance."""
import copy
import math
import re
import struct

from agents_test_support import FIXTURE, compare, expected_state, require, uint
from render_oracle import attachment_bytes

CAMERA = copy.deepcopy(FIXTURE["camera"])
CAMERA["position_m"] = [float(v) for v in CAMERA["position_m"]]
for _field in ("left", "right", "bottom", "top", "near_m", "far_m"):
    CAMERA[_field] = float(CAMERA[_field])


def expected_objects(revision):
    return [{"object_id": n + 1, "entity_uuid": slot["entity_uuid"],
             "generation": 1, "authoring_revision": slot["entity"]["authoring_revision"],
             "transform": slot["entity"]["transform"]}
            for n, slot in enumerate(expected_state(revision)["slots"])]


def expected_pixel(x, y, width, height, revision):
    """Pixel-centre rays and independent rational inverse-yaw/slab intersections."""
    wx, wy = -4 + 8 * (x + .5) / width, 3 - 6 * (y + .5) / height
    best = (0, 0, 1.0)
    closest = math.inf
    for index, obj in enumerate(FIXTURE["objects"][:revision], 1):
        px, py, pz = obj["transform"]["position_m"]
        sx, sy, sz = obj["transform"]["scale"]
        origin = ((.28*(wx-px)-.96*(5-pz))/sx, (wy-py)/sy,
                  (.96*(wx-px)+.28*(5-pz))/sz)
        direction = (.96/sx, 0, -.28/sz)
        near, far, face = -math.inf, math.inf, -1
        miss = False
        for axis, (o, d) in enumerate(zip(origin, direction)):
            if d == 0:
                if not -.5 <= o <= .5:
                    miss = True
                    break
                continue
            low, high = (-.5-o)/d, (.5-o)/d
            entering = axis*2+(1 if d > 0 else 0)
            if low > high:
                low, high = high, low
            if low > near:
                near, face = low, entering
            far = min(far, high)
        if not miss and far >= near and .1 <= near <= 20 and near < closest:
            closest = near
            best = (index, face+1, (near-.1)/19.9)
    return best


def validate_pixels(color, ids, depth, width, height, revision, color_format, phase):
    count = width * height
    require(len(color) == len(ids) == len(depth) == count*4, phase+": exact GPU attachment lengths")
    actual_ids = [item[0] for item in struct.iter_unpack("<I", ids)]
    actual_depth = [item[0] for item in struct.iter_unpack("<f", depth)]
    require(all(0 <= value <= revision for value in actual_ids), phase+": GPU object IDs known everywhere")
    require(all(math.isfinite(value) and 0 <= value <= 1 for value in actual_depth),
            phase+": GPU depth normalized and finite everywhere")
    expected = [expected_pixel(x,y,width,height,revision) for y in range(height) for x in range(width)]
    labels = [value[:2] for value in expected]
    # Edge exclusions are constructed only from the preregistered expected image.
    band = [any(labels[ny*width+nx] != labels[y*width+x]
                for ny in range(max(0,y-1),min(height,y+2))
                for nx in range(max(0,x-1),min(width,x+2)))
            for y in range(height) for x in range(width)]
    core = [index for index in range(count) if not band[index]]
    coverage = [sum(expected[i][0] == identity for i in core) for identity in range(revision+1)]
    require(all(value > 0 for value in coverage), phase+": every expected object and background have interior coverage")
    require(all(actual_ids[i] == expected[i][0] for i in core), phase+": independent GPU object ID mask")
    swizzle = (2,1,0,3) if color_format == "B8G8R8A8_UNORM" else (0,1,2,3)
    require(all(abs(color[i*4+swizzle[channel]] -
                (FIXTURE["face_rgba8"][expected[i][1]-1] if expected[i][0] else FIXTURE["clear_rgba8"])[channel]) <= 1
                for i in core for channel in range(4)), phase+": independent GPU face colors")
    require(all(abs(actual_depth[i]-expected[i][2]) <= 1e-5 for i in core),
            phase+": independent GPU projected depth")
    return {"width":width,"height":height,"tested_pixels":len(core),
            "excluded_expected_edge_pixels":sum(band),"interior_pixels_per_object_and_background":coverage}


def inspect_frame(frame, revision, phase, output, evidence, runtime):
    keys = {"phase","frame_id","world_id","world_revision","simulation_tick","snapshot_sequence",
            "objects","camera","pixel_extent","source_generation","swapchain_generation",
            "image_index","submission_serial","present_result","attachments","copy"}
    require(type(frame) is dict and frame.keys() == keys, phase+": exact frame fields")
    compare(frame["phase"],phase,phase+": fixed phase")
    compare(frame["world_id"],"workshop",phase+": exact world")
    compare(frame["world_revision"],revision,phase+": exact committed revision")
    for key in ("frame_id","simulation_tick","snapshot_sequence","source_generation","swapchain_generation","submission_serial"):
        require(uint(frame[key],phase+"/"+key) > 0,phase+": positive frame source identity")
    require(frame["snapshot_sequence"] == frame["simulation_tick"]+1 and
            frame["snapshot_sequence"] <= runtime["snapshot_sequence"],phase+": actual coherent tick publication")
    require(frame["source_generation"] == frame["swapchain_generation"],
            phase+": attachment source belongs to its submitted swapchain generation")
    require(frame["frame_id"] == frame["submission_serial"],
            phase+": one-submit frame identity matches actual submission")
    require(uint(frame["image_index"],phase+"/image_index") < 3,phase+": bounded swapchain image")
    compare(frame["objects"],expected_objects(revision),phase+": exact independently expected objects")
    compare(frame["camera"],CAMERA,phase+": frozen camera")
    extent=frame["pixel_extent"]
    compare(extent,FIXTURE["pixel_extent"],phase+": frozen viewport extent")
    width,height=extent["width"],extent["height"]
    require(frame["present_result"] in ("VK_SUCCESS","VK_SUBOPTIMAL_KHR"),phase+": actual queued presentation")
    attachments=frame["attachments"]
    require(type(attachments) is dict and attachments.keys()=={"color","object_id","depth"},phase+": attachment roles")
    color,color_format=attachment_bytes(output,attachments["color"],width,height,
                                        {"R8G8B8A8_UNORM","B8G8R8A8_UNORM"},phase+" color",evidence)
    ids,unused=attachment_bytes(output,attachments["object_id"],width,height,{"R32_UINT"},phase+" ID",evidence)
    depth,unused=attachment_bytes(output,attachments["depth"],width,height,{"D32_SFLOAT"},phase+" depth",evidence)
    compare(frame["copy"],{"command":"vkCmdCopyImage","source_generation":frame["source_generation"],
                           "swapchain_generation":frame["swapchain_generation"],"image_index":frame["image_index"],
                           "pixel_extent":extent,"source_format":attachments["color"]["format"],
                           "destination_format":attachments["color"]["format"],"full_extent":True},
            phase+": exact presented color provenance")
    stats=validate_pixels(color,ids,depth,width,height,revision,color_format,phase)
    public=copy.deepcopy(frame)
    for role,data in (("color",color),("object_id",ids),("depth",depth)):
        name="readbacks/"+phase+"-"+role+".bin"
        evidence.retain(name,data)
        public["attachments"][role]["path"]=((evidence.retention_prefix+"/") if evidence.retention_prefix else "")+name
    evidence.retain_json("frames/"+phase+".json",{"frame":public,"pixel_assertions":stats})


def inspect_lifecycle(report):
    trace=report["lifecycle_events"]
    require(type(trace) is list and len(trace)<=600*8+16,"bounded live GPU lifecycle trace")
    submitted,queued,graphics,present={},set(),set(),set()
    retired=set()
    for entry in trace:
        require(type(entry) is dict and entry.keys()=={"event","submission_serial","swapchain_generation","graphics_pending","present_pending"},
                "live lifecycle exact fields")
        event,serial,generation=entry["event"],entry["submission_serial"],entry["swapchain_generation"]
        uint(serial,"live lifecycle serial"); require(uint(generation,"live lifecycle generation")>0,"live generation positive")
        require(type(entry["graphics_pending"]) is bool and type(entry["present_pending"]) is bool,"live pending flags boolean")
        require(event in {"submit","present_queued","graphics_complete","present_complete","retired",
                          "resize_requested","minimize_requested","restore_requested"},"known actual lifecycle event")
        if event=="submit":
            require(serial>0 and serial not in submitted and generation not in retired,"submission owns a live generation once")
            submitted[serial]=generation
        elif event=="present_queued":
            require(submitted.get(serial)==generation and serial not in queued,"presentation follows exact submission")
            queued.add(serial)
        elif event=="graphics_complete":
            require(submitted.get(serial)==generation and serial not in graphics,"graphics completion has exact ownership")
            graphics.add(serial)
        elif event=="present_complete":
            require(submitted.get(serial)==generation and serial in queued and serial not in present,"present completion has exact ownership")
            present.add(serial)
        elif event=="retired":
            owned={s for s,g in submitted.items() if g==generation}
            require(generation not in retired and owned<=graphics and owned<=present,"retirement follows all graphics and present fences")
            require(not entry["graphics_pending"] and not entry["present_pending"],"retired resources have no pending owner")
            retired.add(generation)
        if event!="retired":
            require(entry["graphics_pending"]==(serial in submitted and serial not in graphics),"graphics pending trace is coherent")
            require(entry["present_pending"]==(serial in queued and serial not in present),"presentation pending trace is coherent")
    frames=report["frames"]
    require(set(submitted)==graphics==present and set(submitted.values())<=retired,"all live GPU work completes and retires")
    require([f["submission_serial"] for f in frames]==sorted({f["submission_serial"] for f in frames}),"captured submissions strictly advance")
    for frame in frames:
        require(submitted.get(frame["submission_serial"])==frame["swapchain_generation"] and frame["submission_serial"] in queued,
                "captured frame has the exact submitted and presented generation")
    require(frames[1]["frame_id"]>frames[0]["frame_id"] and frames[1]["simulation_tick"]>frames[0]["simulation_tick"],
            "final GPU frame follows initial frame and real simulation progress")
    presentation=report["runtime"]["presentation"]
    require(presentation["enabled"] is True and presentation["ready"] is True and
            type(presentation["world_revision"]) is int and presentation["world_revision"]==3,
            "final live telemetry confirms the rendered revision")
    require(type(presentation["frame_count"]) is int and
            frames[1]["frame_id"] <= presentation["frame_count"] <= len(submitted) and
            presentation["frame_count"] <= max(submitted),
            "final frame count is bounded by actual submitted work")
    require(type(presentation["snapshot_sequence"]) is int and
            presentation["snapshot_sequence"] >= frames[1]["snapshot_sequence"],
            "final presentation publication covers the retained final capture")
    events=report["window_events"]
    require(type(events) is list and len(events)<=1024,"bounded actual live window events")
    for event in events:
        require(type(event) is dict and event.keys()=={"kind","pixel_extent","minimized","submission_serial","swapchain_generation"},
                "live window event fields")
        require(event["kind"] in ("resized","minimized","restored") and type(event["minimized"]) is bool,"known actual live window transition")
        for key in ("submission_serial","swapchain_generation"):
            uint(event[key],"live window "+key)
        extent=event["pixel_extent"]
        require(type(extent) is dict and extent.keys()=={"width","height"},"live event extent fields")
        uint(extent["width"],"live event width",1024); uint(extent["height"],"live event height",1024)


def inspect_gpu(report, output, evidence):
    device=report["device"]
    require(type(device) is dict and device.keys()=={"name","api_version","driver_version","device_type"},"GPU device allowlist")
    require(device["device_type"] in ("discrete","integrated","discrete_gpu","integrated_gpu",
                                     "DISCRETE_GPU","INTEGRATED_GPU","VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU","VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU"),
            "actual physical GPU is required")
    for key in ("name","api_version","driver_version"):
        require(type(device[key]) is str and 0<len(device[key])<=128 and re.fullmatch(r"[A-Za-z0-9 .()+,_-]+",device[key]),
                "generic GPU metadata has no personal path or opaque device ID")
    evidence.manifest["environment"]["gpu"]=device
    compare(report["validation"],{"enabled":True,"errors":0,"warnings":0,"messages":[]},"actual core and synchronization validation")
    require(type(report["frames"]) is list and len(report["frames"])==2,"two required live captures")
    for frame,revision,phase in zip(report["frames"],(1,3),("initial","final")):
        inspect_frame(frame,revision,phase,output,evidence,report["runtime"])
    inspect_lifecycle(report)
    evidence.retain_json("window-events.json",report["window_events"])
    evidence.retain_json("lifecycle-events.json",report["lifecycle_events"])
