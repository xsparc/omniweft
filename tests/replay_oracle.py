#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Independent literal accepted order, reused identities, content manifest and replay states."""
import argparse
import copy
import json
import struct
import tempfile
from pathlib import Path
from hierarchy_oracle import Evidence as PriorEvidence
from sdk_test_support import ROOT,CHECKS,Failure,digest,exact,require,strict_json,main_guard


def identity(n,g=1):return {'world_id':'workshop','entity_uuid':f'00000007-0000-4000-8000-{n:012x}','generation':g}
def trs(x):return {'position_m':[float(x),0.,0.],'rotation_xyzw':[0.,0.,0.,1.],'scale':[1.,1.,1.]}
def create(name):return {'type':'entity.create','temporary_id':name,'prefab':'builtin.unit_cube'}
def move(target,x):return {'type':'transform.set','target':target,**trs(x)}
def tag(target,value):return {'type':'entity.tags.set','target':target,'tags':[value]}
def erase(target):return {'type':'entity.delete','target':target,'child_policy':'reject_if_children'}
def binding(name,n,g=1):return {'temporary_id':name,**identity(n,g)}


def asset():
    # Reviewed builtin mesh contract, independent of the emitted runtime descriptor.
    faces=[[[.5,-.5,-.5],[.5,.5,-.5],[.5,.5,.5],[.5,-.5,.5]],
           [[-.5,-.5,.5],[-.5,.5,.5],[-.5,.5,-.5],[-.5,-.5,-.5]],
           [[-.5,.5,-.5],[-.5,.5,.5],[.5,.5,.5],[.5,.5,-.5]],
           [[-.5,-.5,.5],[-.5,-.5,-.5],[.5,-.5,-.5],[.5,-.5,.5]],
           [[-.5,-.5,.5],[.5,-.5,.5],[.5,.5,.5],[-.5,.5,.5]],
           [[.5,-.5,-.5],[-.5,-.5,-.5],[-.5,.5,-.5],[.5,.5,-.5]]]
    value={'schema':1,'prefab':'builtin.unit_cube','revision':1,'faces_m':faces,
      'face_rgba8':[[255,64,64,255],[128,32,32,255],[64,255,64,255],[32,128,32,255],[64,64,255,255],[32,32,128,255]],
      'face_triangles':[0,1,2,2,3,0]}
    manifest=json.dumps(value,separators=(',',':'))
    return {'prefab':'builtin.unit_cube','revision':1,'manifest':manifest,'content_sha256':digest(manifest.encode('utf-8'))}


def canonical(s):
    def text(v):
        b=v.encode('ascii');return struct.pack('<I',len(b))+b
    b=bytearray(f"OWOBJ00{s['format_version']}".encode('ascii'))+text(s['world_id'])
    b+=struct.pack('<IIQI',s['seed'],s['max_slots'],s['world_revision'],len(s['slots']))
    for slot in s['slots']:
        e=slot['entity'];b+=slot['entity_uuid'].encode('ascii')+struct.pack('<QBB',slot['generation'],slot['retired'],e is not None)
        if e is None:continue
        t=e['transform'];b+=text(e['prefab'])+struct.pack('<Q',e['authoring_revision'])
        b+=struct.pack('<10d',*(t['position_m']+t['rotation_xyzw']+t['scale']))
        if s['format_version']>=2:b+=b'\0'
        if s['format_version']==3:
            b+=struct.pack('<I',len(e['tags']))
            for value in e['tags']:b+=text(value)
    return b.hex()


def expected():
    states=[]
    # revision, diagnostic format, then (generation, authoring revision, x, tags) by allocator slot.
    for revision,fmt,a,b in [(1,1,(1,1,-2,[]),(1,1,2,[])),
                            (2,3,(1,2,-3,['red']),(1,1,2,[])),
                            (3,3,(1,2,-3,['red']),(2,3,4,[])),
                            (4,3,(2,4,-5,['blue']),(2,3,4,[]))]:
        slots=[]
        for n,(generation,author,x,tags) in enumerate((a,b),1):
            slots.append({'entity_uuid':identity(n)['entity_uuid'],'generation':generation,'retired':False,
              'entity':{'prefab':'builtin.unit_cube','authoring_revision':author,'transform':trs(x),'parent':None,'local_transform':None,'tags':tags}})
        states.append({'format_version':fmt,'world_id':'workshop','seed':7,'max_slots':8,'world_revision':revision,'slots':slots})
    operations=[
      [create('A'),create('B'),move({'temporary_id':'A'},-2),move({'temporary_id':'B'},2)],
      [tag(identity(1),'red'),move(identity(1),-3)],
      [erase(identity(2)),create('C'),move({'temporary_id':'C'},4)],
      [erase(identity(1)),create('D'),move({'temporary_id':'D'},-5),tag({'temporary_id':'D'},'blue')]]
    mappings=[[binding('A',1),binding('B',2)],[],[binding('C',2,2)],[binding('D',1,2)]]
    records=[];receipts=[]
    for i,(tick,transaction) in enumerate([(1,1),(2,2),(2,4),(3,5)]):
        records.append({'sequence':i+1,'boundary_tick':tick,'expected_revision':i,'budget':{'max_operations':4,'max_blob_bytes':0},
                        'operations':operations[i],'created':mappings[i],'checkpoint_hex':canonical(states[i])})
        receipts.append({'status':'committed','durability':'volatile','transaction_id':f'018f7242-4387-7c98-a114-{0x130000+transaction:012x}',
                         'world_revision':i+1,'created':mappings[i],'errors':[]})
    negatives=[{'case':label,'world_returned':False,'code':code,'path':path} for label,code,path in [
      ('late-checkpoint','CHECKPOINT_MISMATCH','/checkpoint'),('required-schema','UNSUPPORTED_SCHEMA','/schema'),
      ('missing-asset','MISSING_ASSET','/assets'),('wrong-generation','INVALID_REPLAY','/created'),
      ('wrong-content','MISSING_ASSET','/assets'),('accepted-order','INVALID_REPLAY','/order')]]
    return {'schema_version':1,'example':'world.replay','seed':7,'verified':True,'rejected_not_recorded':True,
      'log':{'schema':1,'command_schema':'0.1','checkpoint_schema':'OWOBJ001-003','world_id':'workshop','seed':7,'max_slots':8,'assets':[asset()],'records':records},
      'original_receipts':receipts,'original_checkpoints':states,'replayed_checkpoints':copy.deepcopy(states),
      'negative':negatives,'recovery_snapshot':copy.deepcopy(states[-1])}


def validate(value):
    exact(value,expected(),'literal complete accepted replay proof')
    a=value['log']['assets'][0]
    exact(digest(a['manifest'].encode('utf-8')),a['content_sha256'],'builtin descriptor SHA256 identity')
    exact(value['original_checkpoints'],value['replayed_checkpoints'],'all reconstructed checkpoints exact')
    for record,state in zip(value['log']['records'],value['replayed_checkpoints']):
        exact(record['checkpoint_hex'],canonical(state),'independently encoded replay bytes')


class Evidence(PriorEvidence):
    def __init__(self,directory,executable):
        super().__init__(directory,executable)
        for name in ('replay_oracle.py','replay_oracle_test.py','replay_native_test.cpp'):
            p=ROOT/'tests'/name;self.sources[p.relative_to(ROOT).as_posix()]=digest(p.read_bytes())
        self.manifest.update(work_item='PR-013',example='world.replay',source_sha256=self.sources,
          limitations=['Opt-in native in-memory authoring replay; no durable storage or remote replay endpoint.',
                       'Private fresh world only; no live authentication, lease or policy authority restored.',
                       'Builtin cube content manifest only; no general external asset loader.',
                       'CPU exact nonphysics checkpoints; no physics or GPU evidence.'])


def main():
    parser=argparse.ArgumentParser();parser.add_argument('--executable',required=True,type=Path)
    parser.add_argument('--native-test',type=Path);parser.add_argument('--evidence',type=Path);args=parser.parse_args()
    evidence=Evidence(args.evidence,args.executable)
    try:
        evidence.bind_executable('example',args.executable)
        if args.native_test:evidence.bind_executable('native_test',args.native_test)
        with tempfile.TemporaryDirectory(prefix='ow-replay-') as temporary:
            output=Path(temporary)/'example';tail=['--example','world.replay','--headless','--seed','7','--verify','--output']
            result=evidence.run([str(args.executable),*tail,str(output)],['<examples>',*tail,'<output>'])
            require(result.returncode==0,'native replay example executes')
            actual=strict_json((output/'result.json').read_bytes());validate(actual)
            evidence.retain_json('native/actual.json',actual);evidence.retain_json('native/expected.json',expected())
        if args.native_test:
            result=evidence.run([str(args.native_test)],['<replay_native_test>'])
            require(result.returncode==0,'native replay suite executes');value=strict_json(result.stdout.encode('utf-8'))
            require(type(value) is dict and set(value)=={'status','assertions'} and value['status']=='passed'
                    and type(value['assertions']) is int and value['assertions']>=260,'native replay assertion summary')
            evidence.retain_json('native/assertions.json',value)
        evidence.finish('passed');print('replay oracle passed: '+str(len(evidence.manifest['assertions']))+' assertions');return 0
    except Failure as error:evidence.finish('failed',str(error));raise
    except Exception:evidence.finish('failed','unexpected private internal error');raise

if __name__=='__main__':raise SystemExit(main_guard(main))
