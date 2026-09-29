#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Independent literal lifecycle states plus real host uncertainty reconciliation."""
import argparse
import copy
from dataclasses import asdict
import json
from pathlib import Path
import sys
import tempfile
import threading
import time
from unittest.mock import patch

from agents_test_support import compare
from hierarchy_oracle import Evidence as PriorEvidence
from sdk_test_support import (ROOT, CHECKS, Failure, canonical, digest, main_guard,
                              require, strict_json, timestamp)
sys.path.insert(0, str(ROOT/'sdk/python'))
from omniweft_sdk import (CubeGoal, EntityHandle, OutcomeUnknown, ProtocolError,
                          RetryPolicySession, WorkerSupervisor)
from omniweft_sdk import client as transport

ENTITY = '00000007-0000-4000-8000-000000000001'
LIMITS = {'max_operations':4,'max_body_bytes':16384,'retained_bytes':2048,
          'working_bytes':262144,'observation_bytes':4096,'requests':1,'global_requests':2}


def tx(number):
    return f'018f7242-4387-7c98-a115-{number:012x}'


def state(revision, x=3):
    return {'format_version':1,'world_id':'workshop','seed':7,'max_slots':8,
      'world_revision':revision,'slots':[] if revision==0 else [{'entity_uuid':ENTITY,
      'generation':1,'retired':False,'entity':{'prefab':'builtin.unit_cube',
      'authoring_revision':revision,'transform':{'position_m':[float(x),0.,0.],
      'rotation_xyzw':[0.,0.,0.,1.],'scale':[1.,1.,1.]}}}]}


def receipt(identifier, revision):
    return {'status':'committed','durability':'volatile','transaction_id':identifier,
      'world_revision':revision,'created':[{'temporary_id':'A','world_id':'workshop',
      'entity_uuid':ENTITY,'generation':1}] if revision==1 else [],'errors':[]}


def policy(revision):
    return {'principal':'east','world_revision':revision,'next_sequence':revision+1,
      'limits':LIMITS,'usage':{'retained_bytes':384 if revision else 0,'working_bytes':0,
      'requests':0,'global_requests':0},'lower':[1.,-4.,-4.],'upper':[8.,4.,4.]}


def status(value):
    return {'request_id':value.request_id,'generation':value.generation,'state':value.state,
      'expected_revision':value.expected_revision,'late_results':value.late_results,
      'receipt':None if value.receipt is None else value.receipt.to_dict(),
      'error_code':value.error_code,'worker_cleaned':value.worker_cleaned}


def expected_status(number):
    return {'request_id':number,'generation':(1,1,1,2,3,4)[number-1],
      'state':('timed_out','cancelled','crashed','superseded','committed','committed')[number-1],
      'expected_revision':1 if number==6 else 0,'late_results':1 if number in (2,4) else 0,
      'receipt':receipt('018f7242-4387-7c98-a115-00000000150'+str(number),number-4) if number>4 else None,
      'error_code':('DEADLINE_EXPIRED','CANCELLED','WORKER_CRASHED','SUPERSEDED',None,None)[number-1],
      'worker_cleaned':True}


def runtime(value, revision, x=3):
    compare(set(value),{'schema_version','tick_rate_hz','max_catch_up_steps','simulation_tick',
      'snapshot_sequence','overload_count','dropped_ticks','remainder_units','snapshot','presentation'},'runtime fields')
    for key,n in [('schema_version',1),('tick_rate_hz',60),('max_catch_up_steps',4)]:
        compare(value[key],n,'runtime fixed '+key)
    for key in ('simulation_tick','snapshot_sequence','overload_count','dropped_ticks','remainder_units'):
        require(type(value[key]) is int and 0<=value[key]<2**64,'runtime bounded '+key)
    require(value['snapshot_sequence']==value['simulation_tick']+1,'runtime publication sequence')
    require(value['remainder_units']<10**9 and value['overload_count']<=value['dropped_ticks'],'runtime scheduler bounds')
    compare(value['snapshot'],state(revision,x),'complete runtime snapshot')
    compare(value['presentation'],{'enabled':False,'ready':False,'frame_count':0,
        'world_revision':0,'snapshot_sequence':0},'headless presentation')


def validate(report):
    compare(set(report),{'schema_version','example','seed','lane','records','retained',
      'old_submit_rejected','restart_generations','host_exit_code'},'report fields')
    compare([report[k] for k in ('schema_version','example','seed','lane')],
      [1,'agents.worker_failure',7,'cpu'],'report identity')
    compare(report['old_submit_rejected'],True,'late request cannot be submitted')
    compare(report['restart_generations'],[2,4],'provider generations advance')
    compare(report['host_exit_code'],0,'host clean shutdown')
    require(type(report['records']) is list and len(report['records'])==6,'all lifecycle boundaries retained')
    for i,row in enumerate(report['records'],1):
        compare(set(row),{'phase','status','snapshot','policy','progress'},'record fields')
        compare(row['phase'],('timeout','cancelled','crashed','superseded','replacement','restarted')[i-1],'phase')
        compare(row['status'],expected_status(i),'exact terminal lifecycle status')
        revision=max(0,i-4);x=4 if i==6 else 3
        compare(row['snapshot'],state(revision,x),'complete fixture world')
        require(canonical(row['snapshot'])==canonical(state(revision,x)),'complete canonical authoring bytes')
        compare(row['policy'],policy(revision),'full grant, sequence and released leases')
        if i>4:
            compare(row['progress'],None,'no invented pending sample')
            continue
        progress=row['progress']
        compare(set(progress),{'statuses','runtime'},'pending progress fields')
        require(type(progress['statuses']) is list and len(progress['statuses'])==2,'two queryable pending statuses')
        pending={**expected_status(i),'state':'running','error_code':None,'late_results':0,'worker_cleaned':False}
        for value in progress['statuses']:compare(value,pending,'pending status independent of worker work')
        require(type(progress['runtime']) is list and len(progress['runtime'])==2,'two actual runtime samples')
        before,after=progress['runtime']
        runtime(before,0);runtime(after,0)
        require(after['simulation_tick']>before['simulation_tick'],'native simulation advances while worker pending')
    compare(report['retained'],[expected_status(i) for i in range(1,7)],'all original statuses remain queryable')


def idle(observer):
    end=time.monotonic()+3
    while time.monotonic()<end:
        value=observer.policy_status()
        if value.usage.global_requests==0:return value
        time.sleep(.005)
    raise Failure('policy lease failed bounded release')


def public_policy(observer):
    value=idle(observer)
    return {'principal':value.principal,'world_revision':value.world_revision,
      'next_sequence':value.next_sequence,'limits':asdict(value.limits),'usage':asdict(value.usage),
      'lower':list(value.lower),'upper':list(value.upper)}


def wait(supervisor,identifier,states):
    end=time.monotonic()+8
    while time.monotonic()<end:
        value=supervisor.status(identifier)
        if value.state in states and value.worker_cleaned:return value
        time.sleep(.005)
    raise Failure('lifecycle terminal state deadline')


def rejects(call,kind):
    try:call()
    except kind:return True
    raise Failure('unresolved or superseded request accepted')


def uncertain(supervisor,request,observer):
    reached=threading.Event();release=threading.Event();received=[]
    class LostResponse(transport._BoundedResponse):
        def read(self,*args,**kwargs):
            value=super().read(*args,**kwargs)
            if b'"receipt"' in value:
                received.append(time.monotonic())
                reached.set()
                if not release.wait(timeout=2):raise OSError('fixture deadline')
                raise OSError('fixture lost response')
            return value
    idle(observer)
    with patch.object(transport,'_BoundedResponse',LostResponse):
        supervisor.submit(request.request_id)
        try:
            require(reached.wait(timeout=3),'actual committed HTTP response reached loss boundary')
            require(len(received)==1 and time.monotonic()-received[0]<.75,'submission proof starts within original response deadline')
            submitting=status(supervisor.status(request.request_id))
            cancellation=supervisor.cancel(request.request_id)
            blocked=rejects(lambda:supervisor.start(CubeGoal.create('B',3),0),OutcomeUnknown)
            # These use a separate observer client, not the mutation client lock.
            idle(observer)
            before=observer.runtime().to_dict()
            time.sleep(.05)
            idle(observer)
            after=observer.runtime().to_dict()
            require(time.monotonic()-received[0]<.9,'response loss injection remains inside native request budget')
        finally:release.set()
        unknown=status(wait(supervisor,request.request_id,{'outcome_unknown'}))
    return {'submitting':submitting,'cancel_result':cancellation,'replacement_blocked':blocked,
      'runtime':[before,after],'unknown':unknown}


def validate_transport(report):
    compare(set(report),{'first','restarted_generation','after_loss','recovered','after_retry',
      'second','expired','after_expiry','reconciled','fresh','final','policy'},'transport proof fields')
    for name,number,revision,x in [('first',1,1,3),('second',2,2,4)]:
        row=report[name]
        compare(set(row),{'submitting','cancel_result','replacement_blocked','runtime','unknown'},'uncertain boundary fields')
        base={'request_id':number,'generation':number,'state':'submitting','expected_revision':revision-1,
          'late_results':0,'receipt':None,'error_code':None,'worker_cleaned':True}
        compare(row['submitting'],base,'queryable status while actual response held')
        compare(row['cancel_result'],'too_late','post-submit cancellation never claims prevention')
        compare(row['replacement_blocked'],True,'pending authority blocks replacement')
        require(type(row['runtime']) is list and len(row['runtime'])==2,'live native samples while response held')
        for r in row['runtime']:runtime(r,revision,x)
        require(row['runtime'][1]['simulation_tick']>row['runtime'][0]['simulation_tick'],'simulation advances while parent submission pending')
        compare(row['unknown'],{**base,'state':'outcome_unknown','error_code':'OUTCOME_UNKNOWN'},'actual lost response remains unknown')
    compare(report['restarted_generation'],2,'worker restart preserves pending request')
    for key,revision,x in [('after_loss',1,3),('after_retry',1,3),('after_expiry',2,4),('final',3,5)]:
        compare(report[key],state(revision,x),'literal '+key)
    for key,number,revision in [('recovered',1,1),('fresh',3,3)]:
        compare(report[key],{'request_id':number,'generation':1 if number==1 else 2,
          'state':'committed','expected_revision':revision-1,'late_results':0,
          'receipt':receipt(tx(100+number),revision),'error_code':None,'worker_cleaned':True},'exact recovered/fresh status')
    base={'request_id':2,'generation':2,'state':'outcome_unknown','expected_revision':1,
      'late_results':0,'receipt':None,'error_code':'REQUIRES_RESYNC','worker_cleaned':True}
    compare(report['expired'],base,'expired receipt cannot reexecute key')
    compare(report['reconciled'],{**base,'state':'reconciled','error_code':None},'explicit reconciliation releases blocked authoring')
    compare(report['policy'],policy(3),'final usage and sequence')


def transport_cases(executable,evidence):
    session=RetryPolicySession(executable)
    command={'command':['<contention_host>','--world','workshop','--seed','7','--max-slots','8',
      '--session-ttl-ms','30000','--max-runtime-ms','60000','--max-requests','1024'],
      'started_at':timestamp(),'exit_code':None}
    evidence.manifest['commands'].append(command)
    try:
        with session as host:
            client=host.client('east');observer=host.client('east');client.capabilities()
            for principal in ('east','west'):
                info=host.client(principal)._info;evidence.register({'token':info.token,'epoch':info.epoch})
            with WorkerSupervisor(client) as supervisor:
                request=supervisor.start(CubeGoal.create('A',3),0,timeout_ms=5000,transaction_id=tx(101))
                wait(supervisor,request.request_id,{'proposal_ready'})
                first=uncertain(supervisor,request,observer)
                restart=supervisor.restart()
                rejects(lambda:supervisor.start(CubeGoal.create('B',3),1),OutcomeUnknown)
                idle(observer);after_loss=observer.observe().to_dict();idle(observer)
                supervisor.retry(request.request_id)
                recovered=status(wait(supervisor,request.request_id,{'committed'}))
                idle(observer);after_retry=observer.observe().to_dict()
                handle=EntityHandle('workshop',ENTITY,1)
                request=supervisor.start(CubeGoal.move(handle,4),1,timeout_ms=5000,transaction_id=tx(102))
                wait(supervisor,request.request_id,{'proposal_ready'})
                second=uncertain(supervisor,request,observer)
                # Absolute production receipt TTL stays 2,000 ms; no test clock or extension.
                time.sleep(2.2);idle(observer)
                supervisor.retry(request.request_id)
                expired=status(wait(supervisor,request.request_id,{'outcome_unknown'}))
                require(expired['error_code']=='REQUIRES_RESYNC','expired retry surfaces fixed resync requirement')
                idle(observer);after_expiry=observer.observe().to_dict()
                rejects(lambda:supervisor.start(CubeGoal.move(handle,5),2),OutcomeUnknown)
                supervisor.reconcile(request.request_id)
                reconciled=status(wait(supervisor,request.request_id,{'reconciled'}))
                request=supervisor.start(CubeGoal.move(handle,5),2,timeout_ms=5000,transaction_id=tx(103))
                wait(supervisor,request.request_id,{'proposal_ready'});idle(observer)
                supervisor.submit(request.request_id)
                fresh=status(wait(supervisor,request.request_id,{'committed'}))
                idle(observer);final=observer.observe().to_dict();usage=public_policy(observer)
            report={'first':first,'restarted_generation':restart,'after_loss':after_loss,'recovered':recovered,
              'after_retry':after_retry,'second':second,'expired':expired,'after_expiry':after_expiry,
              'reconciled':reconciled,'fresh':fresh,'final':final,'policy':usage}
    finally:command.update(exit_code=session.returncode,finished_at=timestamp())
    require(session.returncode==0,'native host clean stop after lifecycle recovery')
    validate_transport(report);evidence.retain_json('transport/recovery.json',report)


class Evidence(PriorEvidence):
    def __init__(self,directory,executable):
        super().__init__(directory,executable)
        for name in ('lifecycle_oracle.py','lifecycle_oracle_test.py','lifecycle_sdk_test.py','agents_test_support.py'):
            p=ROOT/'tests'/name;self.sources[p.relative_to(ROOT).as_posix()]=digest(p.read_bytes())
        self.manifest.update(work_item='PR-015',example='agents.worker_failure',source_sha256=self.sources,
          limitations=['SDK proposal-only provider supervision; parent and native host survive worker restart.',
            'Existing policy.retry.v1 four-receipt/2000ms volatile retention remains unchanged.',
            'No new authority, remote endpoint, persistent format, OS sandbox, GPU or physics claim.'])


def main():
    parser=argparse.ArgumentParser();parser.add_argument('--executable',type=Path,required=True)
    parser.add_argument('--evidence',type=Path);args=parser.parse_args()
    evidence=Evidence(args.evidence,args.executable)
    try:
        evidence.bind_executable('host',args.executable)
        with tempfile.TemporaryDirectory(prefix='ow-lifecycle-') as temp:
            output=Path(temp)/'example';example=ROOT/'sdk/python/examples/worker_failure.py'
            run=evidence.run([sys.executable,'-B',str(example),'--executable',str(args.executable),
              '--headless','--seed','7','--verify','--output',str(output)],
              ['<python>','-B','sdk/python/examples/worker_failure.py','--executable','<contention_host>',
               '--headless','--seed','7','--verify','--output','<output>'],timeout=45)
            require(run.returncode==0,'runnable lifecycle example succeeds')
            report=strict_json((output/'result.json').read_bytes());validate(report)
            evidence.retain_json('example/actual.json',report)
            evidence.retain_json('example/expected.json',{'states':[state(0)]*4+[state(1),state(2,4)],
                'statuses':[expected_status(i) for i in range(1,7)]})
        transport_cases(args.executable,evidence)
        run=evidence.run([sys.executable,'-B',str(ROOT/'tests/lifecycle_sdk_test.py')],
          ['<python>','-B','tests/lifecycle_sdk_test.py'],timeout=90)
        require(run.returncode==0,'SDK lifecycle edge suite succeeds')
        summary=strict_json(run.stdout.encode('utf-8'))
        require(type(summary) is dict and set(summary)=={'status','assertions'} and summary['status']=='passed'
                and type(summary['assertions']) is int and summary['assertions']==164,'SDK suite bounded result')
        evidence.retain_json('sdk/assertions.json',summary)
        evidence.finish('passed');print('worker lifecycle oracle passed: '+str(len(CHECKS))+' assertions');return 0
    except Exception:
        evidence.finish('failed','lifecycle verification failed; private details withheld')
        raise


if __name__=='__main__':raise SystemExit(main_guard(main))
