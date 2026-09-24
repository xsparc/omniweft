#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Independent literal authored checkpoints, monotonic revisions and diagnostic bytes."""
import argparse
import copy
import struct
import tempfile
from pathlib import Path
from hierarchy_oracle import Evidence as PriorEvidence
from sdk_test_support import ROOT,CHECKS,Failure,digest,exact,require,strict_json,main_guard


def canonical(s):
    def text(v):
        data=v.encode('ascii');return struct.pack('<I',len(data))+data
    data=bytearray(b'OWOBJ003')+text(s['world_id'])
    data+=struct.pack('<IIQI',s['seed'],s['max_slots'],s['world_revision'],len(s['slots']))
    for slot in s['slots']:
        data+=slot['entity_uuid'].encode('ascii')+struct.pack('<QBB',slot['generation'],slot['retired'],True)
        e=slot['entity'];t=e['transform'];data+=text(e['prefab'])+struct.pack('<Q',e['authoring_revision'])
        data+=struct.pack('<10d',*(t['position_m']+t['rotation_xyzw']+t['scale']))
        data+=b'\0'+struct.pack('<I',len(e['tags']))
        for label in e['tags']:data+=text(label)
    return data.hex()


def expected():
    # Explicit fixture table. Each row fixes content, revision metadata and history cursor.
    rows=[
      ('setup',1,1,1,-2,2,['base'],['blue'],1,0,0,1),
      ('edit-one',2,2,1,-4,2,['chair','red'],['blue'],1,1,1,2),
      ('edit-two',3,2,3,-4,4,['chair','red'],['table'],-2,2,2,3),
      ('edit-three',4,4,3,-6,4,['green'],['table'],-2,3,3,4),
      ('undo-three',5,5,3,-4,4,['chair','red'],['table'],-2,3,2,5),
      ('undo-two',6,5,6,-4,2,['chair','red'],['blue'],1,3,1,6),
      ('undo-one',7,7,6,-2,2,['base'],['blue'],1,3,0,7),
      ('redo-one',8,8,6,-4,2,['chair','red'],['blue'],1,3,1,8),
      ('redo-two',9,8,9,-4,4,['chair','red'],['table'],-2,3,2,9),
      ('redo-three',10,10,9,-6,4,['green'],['table'],-2,3,3,10),
      ('physical-rewind',10,10,9,-6,4,['green'],['table'],-2,3,3,10),
      ('external-edit',11,10,11,-6,7,['green'],['table'],1,3,3,10),
      ('stale-undo',11,10,11,-6,7,['green'],['table'],1,3,3,10),
      ('recovery-edit',12,12,11,-8,7,['green'],['table'],1,1,1,12),
      ('recovery-undo',13,13,11,-6,7,['green'],['table'],1,1,0,13)]
    result=[]
    for n,(label,rev,ar,br,ax,bx,at,bt,bs,entries,cursor,head) in enumerate(rows,1):
        slots=[]
        for i,er,x,tags,sx in ((1,ar,ax,at,1),(2,br,bx,bt,bs)):
            slots.append({'entity_uuid':f'00000007-0000-4000-8000-{i:012x}','generation':1,'retired':False,
              'entity':{'prefab':'builtin.unit_cube','authoring_revision':er,'transform':{'position_m':[float(x),0.,0.],
                'rotation_xyzw':[0.,0.,0.,1.],'scale':[float(sx),1.,1.]},'parent':None,'local_transform':None,'tags':tags}})
        state={'format_version':3,'world_id':'workshop','seed':7,'max_slots':8,'world_revision':rev,'slots':slots}
        errors=[]
        if label in ('physical-rewind','stale-undo'):
            errors=[{'code':'UNSUPPORTED_OPERATION' if label=='physical-rewind' else 'REVISION_CONFLICT',
                     'path':'/history/action' if label=='physical-rewind' else '/expected_world_revision',
                     'message':'Authoring history request rejected.'}]
        created=[{'temporary_id':name,'world_id':'workshop','entity_uuid':slots[i]['entity_uuid'],'generation':1}
                  for i,name in enumerate(('A','B'))] if n==1 else []
        r={'status':'rejected' if errors else 'committed','durability':'volatile','transaction_id':f'018f7242-4387-7c98-a114-{0x120000+n:012x}',
           'world_revision':rev,'created':created,'errors':errors}
        result.append({'case':label,'receipt':r,'after':state,'canonical_hex':canonical(state),
                       'history':{'entries':entries,'cursor':cursor,'expected_revision':head}})
    return {'schema_version':1,'example':'world.undo_chain','seed':7,'verified':True,'clear_preserved_world':True,'records':result}


def content(s):
    value=copy.deepcopy(s);del value['world_revision']
    for slot in value['slots']:del slot['entity']['authoring_revision']
    return value


def validate(value):
    exact(value,expected(),'complete literal undo chain')
    rows=value['records']
    for a,b in ((0,6),(2,4),(1,5),(1,7),(2,8),(3,9),(11,14)):
        exact(content(rows[a]['after']),content(rows[b]['after']),'exact authored content round trip')
    for a,b in ((9,10),(11,12)):
        exact(rows[a]['after'],rows[b]['after'],'rejection keeps complete world state')
        exact(rows[a]['history'],rows[b]['history'],'rejection keeps history')


class Evidence(PriorEvidence):
    def __init__(self,directory,executable):
        super().__init__(directory,executable)
        for name in ('undo_oracle.py','undo_oracle_test.py','history_native_test.cpp'):
            p=ROOT/'tests'/name;self.sources[p.relative_to(ROOT).as_posix()]=digest(p.read_bytes())
        self.manifest.update(work_item='PR-012',example='world.undo_chain',source_sha256=self.sources,
          limitations=['Native supported-edit history only; no remote history or new admission authority.',
                       'Transforms require isolated roots; lifecycle and reparenting are unsupported.',
                       'Authored values restore exactly; world/entity revisions remain monotonic.',
                       'Volatile bounded history; no persistence, physical rewind or GPU claim.'])


def main():
    parser=argparse.ArgumentParser();parser.add_argument('--executable',required=True,type=Path)
    parser.add_argument('--native-test',type=Path);parser.add_argument('--evidence',type=Path);args=parser.parse_args()
    evidence=Evidence(args.evidence,args.executable)
    try:
        evidence.bind_executable('example',args.executable)
        if args.native_test:evidence.bind_executable('native_test',args.native_test)
        with tempfile.TemporaryDirectory(prefix='ow-undo-') as temporary:
            output=Path(temporary)/'example';tail=['--example','world.undo_chain','--headless','--seed','7','--verify','--output']
            result=evidence.run([str(args.executable),*tail,str(output)],['<examples>',*tail,'<output>'])
            require(result.returncode==0,'native undo example executes')
            actual=strict_json((output/'result.json').read_bytes());validate(actual)
            evidence.retain_json('native/actual.json',actual);evidence.retain_json('native/expected.json',expected())
        if args.native_test:
            result=evidence.run([str(args.native_test)],['<history_native_test>'])
            require(result.returncode==0,'native history suite executes')
            value=strict_json(result.stdout.encode('utf-8'))
            require(type(value) is dict and set(value)=={'status','assertions'} and value['status']=='passed'
                    and type(value['assertions']) is int and value['assertions']>=90,'native history assertion summary')
            evidence.retain_json('native/assertions.json',value)
        evidence.finish('passed');print('undo oracle passed: '+str(len(evidence.manifest['assertions']))+' assertions');return 0
    except Failure as error:evidence.finish('failed',str(error));raise
    except Exception:evidence.finish('failed','unexpected private internal error');raise

if __name__=='__main__':raise SystemExit(main_guard(main))
