#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Independent full-state persistence oracle and abrupt child-process crash matrix."""
import argparse,copy,json,tempfile,shutil,struct,hashlib
from pathlib import Path
from replay_oracle import canonical,identity,trs,binding
from hierarchy_oracle import Evidence as PriorEvidence
from sdk_test_support import ROOT,CHECKS,Failure,digest,exact,require,strict_json,main_guard

def state(n):
    x=-2 if n==1 else (4 if n in (2,3) else n)
    tags=['blue'] if n>=3 else []
    slot={'entity_uuid':identity(1)['entity_uuid'],'generation':1 if n==1 else 2,'retired':False,
      'entity':{'prefab':'builtin.unit_cube','authoring_revision':n,'transform':trs(x),'parent':None,'local_transform':None,'tags':tags}}
    s={'format_version':3 if tags else 1,'world_id':'workshop','seed':7,'max_slots':8,'world_revision':n,'slots':[slot]}
    return {'snapshot':s,'canonical_hex':canonical(s),'durable_revision':n,'high_water':n,'low_water':max(1,n-3),'recovery_required':False}

def receipt(n):
    return {'status':'committed','durability':'durable','transaction_id':f'018f7242-4387-7c98-a114-{0x140000+n:012x}',
      'world_revision':n,'created':[binding('A',1)] if n==1 else ([binding('B',1,2)] if n==2 else []),'errors':[]}

def result(n,receipt_n=None,replayed=False,code=''):
    return {'receipt':None if receipt_n is None else receipt(receipt_n),'replayed':replayed,'durable_revision':n,
      'high_water':n,'low_water':max(1,n-3),'recovery_required':False,'code':code}

def expected():
    return {'schema_version':1,'example':'world.crash_recovery','seed':7,'action':'baseline','verified':True,
      'states':[state(n) for n in range(1,6)],'receipts':[result(n,n) for n in range(1,6)],
      'retry':result(5,5,True),'mismatch':result(5,code='IDEMPOTENCY_CONFLICT'),
      'expired':result(5,code='REQUIRES_RESYNC'),'compacted':result(5),'recovered':state(5),
      'lookup':result(5,5,True),'epoch_preserved':True}

def validate(value):
    exact(value,expected(),'literal complete durable authoring proof')
    for n,s in enumerate(value['states'],1):exact(s['canonical_hex'],canonical(s['snapshot']),'independent durable checkpoint bytes')

PHASES=['before_blob_flush','before_journal_flush','after_journal_flush','before_ack','after_ack',
        'before_checkpoint_flush','after_checkpoint_flush','before_checkpoint_replace','after_checkpoint_replace',
        'before_journal_reclaim','after_journal_reclaim']

def validate_crashes(value):
    exact(set(value),{'schema_version','cases'},'crash proof shape');exact(value['schema_version'],1,'crash proof schema')
    exact(len(value['cases']),len(PHASES),'all crash boundaries retained')
    for phase,c in zip(PHASES,value['cases']):
        exact(set(c),{'phase','exit_code','acknowledgement','epoch_preserved','evicted','recovered','lookup','retry','after_retry'},'crash case shape')
        exact(c['phase'],phase,'ordered crash boundary');exact(c['exit_code'],86,'abrupt process exit observed')
        exact(c['acknowledgement'],result(2,2) if phase=='after_ack' else None,'receipt actually delivered only after acknowledgement boundary')
        compact='checkpoint' in phase or 'reclaim' in phase
        n=c['recovered']['high_water'];allowed=(5,) if compact else ((1,) if phase=='before_blob_flush' else ((1,2) if phase=='before_journal_flush' else (2,)))
        exact(c['epoch_preserved'],True,'private precrash epoch is restored')
        exact(c['evicted'],result(5,code='REQUIRES_RESYNC') if compact else None,'compacted low-water remains enforced')
        require(type(n) is int and n in allowed,'predeclared allowed complete durable prefix')
        exact(c['recovered'],state(n),'complete recovered crash state')
        exact(c['lookup'],result(n,n,True),'unacknowledged durable receipt is discoverable')
        target=5 if compact else 2
        exact(c['retry'],result(target,target,n==target),'original uncertain key is committed or replayed exactly once')
        exact(c['after_retry'],state(target),'repaired store advances exactly once after uncertain retry')

NEGATIVES=['truncated-tail','journal-hash','journal-schema','checkpoint-hash','missing-asset','frame-length','frame-high-water','header-digest']
def validate_negatives(value):
    exact(set(value),{'schema_version','cases'},'negative proof shape');exact(value['schema_version'],1,'negative schema')
    exact(len(value['cases']),len(NEGATIVES),'all storage corruptions retained')
    for label,c in zip(NEGATIVES,value['cases']):
        expected_case={'case':label,'preserved':True,'code':'CORRUPT_STORE','recovery':state(2),'continued':state(2)}
        if label=='truncated-tail':expected_case={'case':label,'preserved':True,'code':'RECOVERED_PREFIX','recovery':state(1),'continued':state(2)}
        exact(c,expected_case,'complete storage rejection/recovery proof')

def corrupt_asset(frame):
    data=bytearray(frame);offset=0;count=0
    while offset<len(data):
        require(data[offset:offset+8]==b'OWJRN001','independent frame magic before fixture mutation')
        size=struct.unpack_from('<I',data,offset+8)[0];end=offset+52+size
        require(end+40<=len(data),'complete frame before fixture mutation')
        exact(hashlib.sha256(data[offset:offset+20]).digest(),bytes(data[offset+20:offset+52]),'independent header checksum before semantic mutation')
        payload=bytes(data[offset+52:end]);needle=b'builtin.unit_cube'
        require(needle in payload,'inline builtin identity exists')
        payload=payload.replace(needle,b'builtin.unit_cubX')
        data[offset+52:end]=payload;data[end:end+32]=hashlib.sha256(data[offset:end]).digest()
        offset=end+40;count+=1
    require(count>0,'at least one semantic asset corruption');return bytes(data)

class Evidence(PriorEvidence):
    def __init__(self,directory,executable):
        super().__init__(directory,executable)
        for name in ('persistence_oracle.py','persistence_oracle_test.py','persistence_native_test.cpp','replay_oracle.py'):
            p=ROOT/'tests'/name;self.sources[p.relative_to(ROOT).as_posix()]=digest(p.read_bytes())
        self.manifest.update(work_item='PR-014',example='world.crash_recovery',source_sha256=self.sources,
          limitations=['Bounded native local-filesystem process-crash durability; no arbitrary power/device failure guarantee.',
            'Eight total commits/four operations/eight slots/four durable receipts; no unbounded history or format migration.',
            'One trusted native owner; existing HTTP/SDK profiles remain volatile.',
            'Configured byte quota is tested without filling the host filesystem.',
            'CPU nonphysics state only; no GPU/physics evidence.'])

def main():
    parser=argparse.ArgumentParser();parser.add_argument('--executable',required=True,type=Path)
    parser.add_argument('--native-test',type=Path);parser.add_argument('--evidence',type=Path);args=parser.parse_args()
    evidence=Evidence(args.evidence,args.executable);counter=0
    try:
        evidence.bind_executable('example',args.executable)
        if args.native_test:evidence.bind_executable('native_test',args.native_test)
        with tempfile.TemporaryDirectory(prefix='ow-persistence-') as temporary:
            root=Path(temporary)
            def run(action,store,phase=None,seq=None,failed=False):
                nonlocal counter
                counter+=1;output=root/f'out-{counter}'
                tail=['--example','world.crash_recovery','--headless','--seed','7','--verify','--action',action,'--store',str(store),'--output',str(output),'--epoch-witness',str(store.with_suffix('.epoch'))]
                public=[*tail];public[public.index('--store')+1]='<private-store>';public[public.index('--output')+1]='<output>';public[public.index('--epoch-witness')+1]='<private-epoch-witness>'
                if phase:tail+=['--crash-at',phase];public+=['--crash-at',phase]
                if seq:tail+=['--sequence',str(seq)];public+=['--sequence',str(seq)]
                r=evidence.run([str(args.executable),*tail],['<examples>',*public])
                if phase:
                    exact(r.returncode,86,'fault hook terminates distinct native child')
                    if phase=='after_ack':
                        ack=strict_json(r.stdout.encode('utf-8'));exact(ack,{'ack':result(2,2)},'parent observed complete durable acknowledgement before child exited');return ack['ack']
                    exact(r.stdout,'','no acknowledgement delivered before selected crash');return None
                if failed:
                    exact(r.returncode,2,'corrupt selected store fails closed');exact(r.stderr.strip(),'CORRUPT_STORE','fixed sanitized storage error');return None
                require(r.returncode==0,'native persistence action executes')
                report=strict_json((output/'result.json').read_bytes())
                if action!='baseline':
                    exact(set(report),{'schema_version','example','seed','action','verified','result','state','epoch_preserved'},'sanitized complete action report shape')
                    for k,v in {'schema_version':1,'example':'world.crash_recovery','seed':7,'action':action,'verified':True,'epoch_preserved':True}.items():exact(report[k],v,'fixed action report header')
                if action in ('create','advance'):exact(strict_json(r.stdout.encode('utf-8')),{'ack':report['result']},'complete delivered receipt equals recorded response')
                else:exact(r.stdout,'','no unreviewed process output')
                return report
            actual=run('baseline',root/'baseline');validate(actual)
            evidence.retain_json('native/actual.json',actual);evidence.retain_json('native/expected.json',expected())
            cases=[]
            for phase in PHASES:
                store=root/phase;created=run('create',store);exact(created['state'],state(1),'initial durable fixture')
                compact='checkpoint' in phase or 'reclaim' in phase
                if compact:
                    for sequence in range(2,6):
                        advanced=run('advance',store,seq=sequence);exact(advanced['state'],state(sequence),'precompaction receipt-eviction fixture')
                ack=run('compact' if compact else 'advance',store,phase)
                recovered=run('inspect',store);n=recovered['state']['high_water'];retry=run('retry',store,seq=5 if compact else 2)
                evicted=run('retry',store,seq=1)['result'] if compact else None
                cases.append({'phase':phase,'exit_code':86,'acknowledgement':ack,'epoch_preserved':recovered['epoch_preserved'] and retry['epoch_preserved'],'evicted':evicted,'recovered':recovered['state'],'lookup':recovered['result'],
                              'retry':retry['result'],'after_retry':retry['state']})
            crashes={'schema_version':1,'cases':cases};validate_crashes(crashes);evidence.retain_json('native/crashes.json',crashes)
            negatives=[]
            for label in NEGATIVES:
                store=root/('negative-'+label);run('create',store);run('advance',store)
                journal=store/'journal.bin';checkpoint=store/'checkpoint.bin'
                original={p.name:p.read_bytes() for p in store.iterdir() if p.is_file()}
                if label=='truncated-tail':journal.write_bytes(original['journal.bin'][:-10])
                elif label=='journal-hash':
                    b=bytearray(original['journal.bin']);b[-41]^=1;journal.write_bytes(b)
                elif label=='journal-schema':
                    b=bytearray(original['journal.bin']);b[7]=ord('9');journal.write_bytes(b)
                elif label=='checkpoint-hash':
                    b=bytearray(original['checkpoint.bin']);b[-41]^=1;checkpoint.write_bytes(b)
                elif label=='missing-asset':journal.write_bytes(corrupt_asset(original['journal.bin']))
                else:
                    b=bytearray(original['journal.bin']);at=0;last=0
                    while at<len(b):last=at;at+=struct.unpack_from('<I',b,at+8)[0]+92
                    exact(at,len(b),'independent complete frame traversal')
                    if label=='frame-length':struct.pack_into('<I',b,last+8,struct.unpack_from('<I',b,last+8)[0]+65536)
                    elif label=='frame-high-water':b[last+12]^=1
                    else:b[last+20]^=1
                    journal.write_bytes(b)
                altered={p.name:p.read_bytes() for p in store.iterdir() if p.is_file()}
                if label=='truncated-tail':
                    recovered=run('inspect',store);exact(recovered['state'],state(1),'torn tail recovers last complete prefix')
                    preserved=checkpoint.read_bytes()==original['checkpoint.bin']
                    negatives.append({'case':label,'preserved':preserved,'code':'RECOVERED_PREFIX','recovery':recovered['state'],'continued':run('advance',store)['state']})
                else:
                    run('inspect',store,failed=True)
                    preserved=altered=={p.name:p.read_bytes() for p in store.iterdir() if p.is_file()}
                    require(preserved,'invalid selected package is not rewritten')
                    for name,data in original.items():(store/name).write_bytes(data)
                    recovered=run('inspect',store)
                    negatives.append({'case':label,'preserved':preserved,'code':'CORRUPT_STORE','recovery':recovered['state'],'continued':run('retry',store,seq=2)['state']})
            negative_proof={'schema_version':1,'cases':negatives};validate_negatives(negative_proof);evidence.retain_json('native/negative.json',negative_proof)

        if args.native_test:
            r=evidence.run([str(args.native_test)],['<persistence_native_test>']);require(r.returncode==0,'native persistence suite executes')
            value=strict_json(r.stdout.encode('utf-8'))
            require(type(value) is dict and set(value)=={'status','case_set','assertions'} and value['status']=='passed' and type(value['assertions']) is int and value['assertions']>=272 and value['case_set']=='persistence-v1','native assertion summary')
            evidence.retain_json('native/assertions.json',value)
        evidence.finish('passed');print('persistence oracle passed: '+str(len(evidence.manifest['assertions']))+' assertions');return 0
    except Failure as error:evidence.finish('failed',str(error));raise
    except Exception:evidence.finish('failed','unexpected private internal error');raise
if __name__=='__main__':raise SystemExit(main_guard(main))

