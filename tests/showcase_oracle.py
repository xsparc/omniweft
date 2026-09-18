#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Independent room states and pixel-ray oracle; never imports authoring values."""
import argparse
import copy
import importlib.util
import math
from pathlib import Path
import re
import struct
import sys
import tempfile
import time

from agents_test_support import compare, canonical, uint
from agents_gpu_oracle import CAMERA, inspect_lifecycle
from render_oracle import attachment_bytes
from sdk_test_support import Evidence, Failure, ROOT, require, strict_json, main_guard, digest

# Preregistered review fixture. Positions/scales are meters; depth tolerance is
# normalized clip depth. These values are not read from the example or reports.
NAMES=("floor","back","side","table","stool")
INITIAL=((2.5,-1.1,0),(2.5,.2,-.9),(1.35,.2,0),(2.15,-.55,.35),(3.2,-.7,.5))
FINAL=INITIAL[:3]+((3.1,-.55,.15),(2,-.7,.5))
SCALES=((2.5,.2,2),(2.5,2.4,.2),(.2,2.4,2),(.8,.9,.7),(.4,.6,.5))
ROTATIONS=((0,0,0,1),(0,1,0,0),(0,0,0,1),(0,.6,0,.8),(0,0,0,1))
LIMITS={"max_operations":4,"max_body_bytes":16384,"retained_bytes":2048,
        "working_bytes":262144,"observation_bytes":4096,"requests":1,"global_requests":2}
COLORS=((255,64,64,255),(128,32,32,255),(64,255,64,255),
        (32,128,32,255),(64,64,255,255),(32,32,128,255))
CLEAR=(16,20,24,255)


def expected_state(revision):
    count=(0,2,4,5,5)[revision]
    positions=FINAL if revision==4 else INITIAL
    return {"format_version":1,"world_id":"workshop","seed":7,"max_slots":8,
        "world_revision":revision,"slots":[{"entity_uuid":f"00000007-0000-4000-8000-{n+1:012x}",
        "generation":1,"retired":False,"entity":{"prefab":"builtin.unit_cube",
        "authoring_revision":4 if revision==4 and n>=3 else (1,1,2,2,3)[n],
        "transform":{"position_m":list(map(float,positions[n])),"rotation_xyzw":list(map(float,ROTATIONS[n])),
                     "scale":list(map(float,SCALES[n]))}}} for n in range(count)]}


def expected_receipt(index):
    revision=(1,2,3,3,4)[index]
    created=[]
    if index<3:
        for n in ((0,1),(2,3),(4,))[index]:
            created.append({"temporary_id":NAMES[n],"world_id":"workshop",
                "entity_uuid":f"00000007-0000-4000-8000-{n+1:012x}","generation":1})
    return {"status":"rejected" if index==3 else "committed","durability":"volatile",
        "transaction_id":f"018f7242-4387-7c98-a118-{index+1:012x}","world_revision":revision,
        "created":created,"errors":[{"code":"NOT_AUTHORIZED","path":"/scope",
        "message":"Complete cube bounds must fit the host-issued write region.","operation_index":1}] if index==3 else []}


def state(actual,revision,label):
    expected=expected_state(revision)
    compare(actual,expected,label)
    require(canonical(actual)==canonical(expected),label+": complete canonical bytes")


def policy(actual,index):
    revision=(1,2,3,3,4)[index]
    compare(actual,{"principal":"east","world_revision":revision,"next_sequence":index+2,
        "limits":LIMITS,"usage":{"retained_bytes":(768,1536,1920,1920,1920)[index],
        "working_bytes":0,"requests":0,"global_requests":0},"lower":[1.,-4.,-4.],"upper":[8.,4.,4.]},
        "unchanged grant and recovered usage")


def runtime(value,revision,gpu):
    compare(set(value),{"schema_version","tick_rate_hz","max_catch_up_steps","simulation_tick",
        "snapshot_sequence","overload_count","dropped_ticks","remainder_units","snapshot","presentation"},"runtime fields")
    for name,expected in (("schema_version",1),("tick_rate_hz",60),("max_catch_up_steps",4)):
        compare(value[name],expected,"fixed runtime "+name)
    for key in ("simulation_tick","snapshot_sequence","overload_count","dropped_ticks","remainder_units"):
        require(type(value[key]) is int and 0<=value[key]<(1<<64),"bounded runtime "+key)
    require(value["remainder_units"]<1000000000 and value["overload_count"]<=value["dropped_ticks"],"bounded fractional phase and overload debt")
    require(value["snapshot_sequence"]==value["simulation_tick"]+1,"coherent immutable publication")
    state(value["snapshot"],revision,"runtime authoritative snapshot")
    p=value["presentation"]
    compare(set(p),{"enabled","ready","frame_count","world_revision","snapshot_sequence"},"presentation fields")
    compare(p["enabled"],gpu,"requested presentation lane")
    require(type(p["ready"]) is bool,"ready boolean")
    for key in ("frame_count","world_revision","snapshot_sequence"):
        require(type(p[key]) is int and p[key]>=0,"presentation counter "+key)
    require(p["world_revision"]<=revision and p["snapshot_sequence"]<=value["snapshot_sequence"],"presentation cannot lead publication")
    if not gpu:
        compare(p,{"enabled":False,"ready":False,"frame_count":0,"world_revision":0,"snapshot_sequence":0},"headless does not fabricate GPU")


def validate(report,gpu):
    compare(set(report),{"schema_version","example","seed","lane","initial","records"},"example allowlist")
    compare((report["schema_version"],report["example"],report["seed"],report["lane"]),
            (1,"showcase.ai_blocks",7,"gpu" if gpu else "cpu"),"example identity")
    state(report["initial"],0,"initial empty world")
    require(type(report["records"]) is list and len(report["records"])==5,"all five transaction boundaries retained")
    for index,row in enumerate(report["records"]):
        compare(set(row),{"phase","receipt","snapshot","runtime","policy"},"record allowlist")
        compare(row["phase"],("building","building","assembled","rejected","rearranged")[index],"phase")
        compare(row["receipt"],expected_receipt(index),"exact receipt")
        revision=(1,2,3,3,4)[index]
        state(row["snapshot"],revision,"complete fixture state")
        policy(row["policy"],index)
        runtime(row["runtime"],revision,gpu)
        if gpu and index in (2,4):
            require(row["runtime"]["presentation"]["world_revision"]==revision,"barrier waits for actual scene presentation")
    require(canonical(report["records"][2]["snapshot"])==canonical(report["records"][3]["snapshot"]),"invalid mixed plan rolls back every byte")


def expected_pixel(x,y,width,height,revision):
    wx,wy=-4+8*(x+.5)/width,3-6*(y+.5)/height
    best=(0,0,1.)
    closest=math.inf
    positions=FINAL if revision==4 else INITIAL
    for n,(position,scale) in enumerate(zip(positions,SCALES)):
        # Independent inverse yaw, using preregistered rational cosine/sine.
        cosine,sine=((-1.,0.) if n==1 else (.28,.96) if n==3 else (1.,0.))
        px,py,pz=position;sx,sy,sz=scale
        origin=((cosine*(wx-px)-sine*(5-pz))/sx,(wy-py)/sy,
                (sine*(wx-px)+cosine*(5-pz))/sz)
        direction=(sine/sx,0,-cosine/sz)
        near,far,face=-math.inf,math.inf,-1
        for axis,(o,d) in enumerate(zip(origin,direction)):
            if d==0:
                if not -.5<=o<=.5:
                    far=-math.inf;break
            else:
                low,high=(-.5-o)/d,(.5-o)/d
                entering=axis*2+(1 if d>0 else 0)
                if low>high:low,high=high,low
                if low>near:near,face=low,entering
                far=min(far,high)
        if far>=near and .1<=near<=20 and near<closest:
            closest=near;best=(n+1,face+1,(near-.1)/19.9)
    return best


def pixels(color,ids,depth,width,height,revision,color_format):
    count=width*height
    require(len(color)==len(ids)==len(depth)==count*4,"exact attachment lengths")
    actual_ids=[x[0] for x in struct.iter_unpack('<I',ids)]
    actual_depth=[x[0] for x in struct.iter_unpack('<f',depth)]
    require(all(0<=v<=5 for v in actual_ids),"known object IDs everywhere")
    require(all(math.isfinite(v) and 0<=v<=1 for v in actual_depth),"finite normalized depth everywhere")
    expected=[expected_pixel(x,y,width,height,revision) for y in range(height) for x in range(width)]
    labels=[v[:2] for v in expected]
    core=[y*width+x for y in range(height) for x in range(width) if all(
        labels[ny*width+nx]==labels[y*width+x] for ny in range(max(0,y-1),min(height,y+2))
        for nx in range(max(0,x-1),min(width,x+2)))]
    coverage=[sum(expected[i][0]==identity for i in core) for identity in range(6)]
    require(all(v>0 for v in coverage),"all five objects and background have independently expected interiors")
    require(all(actual_ids[i]==expected[i][0] for i in core),"independent whole-scene object mask")
    swizzle=(2,1,0,3) if color_format=='B8G8R8A8_UNORM' else (0,1,2,3)
    require(all(abs(color[4*i+swizzle[c]]-(COLORS[expected[i][1]-1] if expected[i][0] else CLEAR)[c])<=1
        for i in core for c in range(4)),"independent face colors within one UNORM byte")
    require(all(abs(actual_depth[i]-expected[i][2])<=1e-5 for i in core),"independent normalized depth within 1e-5")
    return {"tested_pixels":len(core),"excluded_expected_edge_pixels":count-len(core),"coverage":coverage}


def gpu_frames(report,output,evidence):
    device=report['device']
    compare(set(device),{'name','api_version','driver_version','device_type'},'generic device fields')
    require(device['device_type'] in ('discrete_gpu','integrated_gpu'),'physical GPU required')
    for key in ('name','api_version','driver_version'):
        require(type(device[key]) is str and re.fullmatch(r'[A-Za-z0-9 .()+,_-]{1,128}',device[key]),'safe generic GPU metadata')
    evidence.manifest['environment']['gpu']=device
    compare(report['validation'],{'enabled':True,'errors':0,'warnings':0,'messages':[]},'actual clean synchronization/core validation')
    require(len(report['frames'])==2,'two complete captures')
    for frame,revision,phase in zip(report['frames'],(3,4),('initial','final')):
        compare(set(frame),{'phase','frame_id','world_id','world_revision','simulation_tick','snapshot_sequence','objects','camera','pixel_extent','source_generation','swapchain_generation','image_index','submission_serial','present_result','attachments','copy'},'frame allowlist')
        for key in ('frame_id','simulation_tick','snapshot_sequence','source_generation','swapchain_generation','submission_serial'):
            require(uint(frame[key],'frame '+key)>0,'positive frame source '+key)
        require(uint(frame['image_index'],'frame image index')<3,'bounded swapchain image index')
        compare((frame['phase'],frame['world_id'],frame['world_revision']),(phase,'workshop',revision),'capture identity')
        objects=[{'object_id':n+1,'entity_uuid':s['entity_uuid'],'generation':1,'authoring_revision':s['entity']['authoring_revision'],'transform':s['entity']['transform']} for n,s in enumerate(expected_state(revision)['slots'])]
        compare(frame['objects'],objects,'capture transforms and identities')
        compare(frame['camera'],CAMERA,'frozen camera');compare(frame['pixel_extent'],{'width':320,'height':240},'frozen extent')
        require(frame['snapshot_sequence']==frame['simulation_tick']+1 and frame['snapshot_sequence']<=report['runtime']['snapshot_sequence'],'capture immutable publication')
        require(frame['source_generation']==frame['swapchain_generation'] and frame['frame_id']==frame['submission_serial'],'source submission generation')
        require(frame['present_result'] in ('VK_SUCCESS','VK_SUBOPTIMAL_KHR'),'real queued presentation')
        compare(set(frame['attachments']),{'color','object_id','depth'},'three attachment roles')
        compare(frame['copy'],{'command':'vkCmdCopyImage','source_generation':frame['source_generation'],'swapchain_generation':frame['swapchain_generation'],'image_index':frame['image_index'],'pixel_extent':frame['pixel_extent'],'source_format':frame['attachments']['color']['format'],'destination_format':frame['attachments']['color']['format'],'full_extent':True},'submitted presentation copy')
        evidence.retention_prefix=phase
        color,fmt=attachment_bytes(output,frame['attachments']['color'],320,240,{'R8G8B8A8_UNORM','B8G8R8A8_UNORM'},phase+' color',evidence)
        ids,_=attachment_bytes(output,frame['attachments']['object_id'],320,240,{'R32_UINT'},phase+' id',evidence)
        depth,_=attachment_bytes(output,frame['attachments']['depth'],320,240,{'D32_SFLOAT'},phase+' depth',evidence)
        evidence.retain_json('pixels.json',pixels(color,ids,depth,320,240,revision,fmt))
        for role,data in (('color',color),('object_id',ids),('depth',depth)):
            name=role+'.bin'
            evidence.retain(name,data)
            frame['attachments'][role]['path']=phase+'/'+name
        evidence.retention_prefix=''
    inspect_lifecycle(report,final_revision=4)


def main():
    parser=argparse.ArgumentParser();parser.add_argument('--executable',type=Path,required=True)
    mode=parser.add_mutually_exclusive_group(required=True);mode.add_argument('--headless',action='store_true');mode.add_argument('--gpu',action='store_true')
    parser.add_argument('--evidence',type=Path);args=parser.parse_args()
    executable=args.executable.resolve();evidence=Evidence(args.evidence,executable)
    evidence.manifest.update(work_item='PR-008',example='showcase.ai_blocks',mode='gpu' if args.gpu else 'cpu',lanes={'cpu':'running','gpu':'running' if args.gpu else 'not_run'})
    sources=[p for d in ('src','include','sdk/python','tests','cmake','toolchains') for p in (ROOT/d).rglob('*') if p.is_file() and p.suffix in ('.py','.hpp','.cpp','.json','.cmake','.txt') and '__pycache__' not in p.parts]
    sources.extend([ROOT/'CMakeLists.txt',ROOT/'CMakePresets.json'])
    sources.extend(p for p in (ROOT/'shaders').iterdir() if p.suffix in ('.vert','.frag','.json'))
    evidence.manifest['source_sha256']={p.relative_to(ROOT).as_posix():digest(p.read_bytes()) for p in sources}
    if args.gpu and args.evidence:
        require(evidence.manifest['environment']['build']['vulkan_enabled'],'GPU proof uses configured Vulkan build')
    try:
        with tempfile.TemporaryDirectory(prefix='ow-room-') as temporary:
            root=Path(temporary);output=root/'cli'
            result=evidence.run([sys.executable,str(ROOT/'sdk/python/examples/ai_blocks.py'),'--executable',str(executable),'--output',str(output),'--gpu' if args.gpu else '--headless'],['<python>','sdk/python/examples/ai_blocks.py','--executable','<policy-host>','--output','<fresh-output>','--gpu' if args.gpu else '--headless'],timeout=45)
            require(result.returncode==0,'standalone room example exits successfully')
            report=strict_json((output/'result.json').read_bytes());validate(report,args.gpu)
            native=strict_json((output/'native/result.json').read_bytes())
            compare(set(native),{'schema_version','example','status','runtime','device','validation','frames','window_events','lifecycle_events','errors'},'native report allowlist')
            compare((native['schema_version'],native['example'],native['status'],native['errors']),(1,'showcase.ai_blocks','passed',[]),'native completion')
            runtime(native['runtime'],4,args.gpu)
            if args.gpu:gpu_frames(native,output/'native',evidence)
            else:
                compare(native['device'],{},'headless has no GPU identity');compare(native['validation'],{'enabled':False,'errors':0,'warnings':0,'messages':[]},'headless validation not GPU proof')
                for key in ('frames','window_events','lifecycle_events'):compare(native[key],[],'headless '+key)
            evidence.retain_json('example.json',report);evidence.retain_json('native.json',native)
            # A second real run observes authoritative state directly from an
            # independent client at each generator barrier, before release.
            sys.path.insert(0,str(ROOT/'sdk/python'))
            spec=importlib.util.spec_from_file_location('room_example',ROOT/'sdk/python/examples/ai_blocks.py');example=importlib.util.module_from_spec(spec);spec.loader.exec_module(example)
            seen=[]
            def inspect(phase,receipt,observer):
                index=len(seen);revision=(1,2,3,3,4)[index]
                compare(receipt.to_dict(),expected_receipt(index),'barrier receipt')
                current=observer.observe().to_dict();state(current,revision,'independent client at held phase')
                seen.append(phase);evidence.retain_json(f'held-{index}.json',current)
            example.run(executable,root/'held',inspect=inspect)
            compare(seen,['building','building','assembled','rejected','rearranged'],'all independent phase observations')
        require(all(digest((ROOT/p).read_bytes())==sha for p,sha in evidence.manifest['source_sha256'].items()),'tested source remains unchanged')
        evidence.manifest['lanes']['gpu']='passed' if args.gpu else 'not_run';evidence.finish('passed')
        print('showcase oracle: exact room, rejection/recovery, independent barriers'+(' and physical GPU pixels' if args.gpu else '')+' passed')
        return 0
    except Exception:
        evidence.manifest['lanes']['gpu']='failed' if args.gpu else 'not_run';evidence.finish('failed','room oracle failed')
        raise


if __name__=='__main__':
    raise SystemExit(main_guard(main))
