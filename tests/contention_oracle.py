#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Literal contention states and real opt-in SDK/transport recovery evidence."""
import argparse
import copy
from dataclasses import asdict
import json
import re
import socket
import tempfile
import threading
import time
from contextlib import contextmanager
from pathlib import Path
from unittest.mock import patch
from hierarchy_oracle import Evidence as PriorEvidence
from sdk_test_support import (ROOT, CHECKS, Failure, digest, encoded, exact, exchange, main_guard,
                              request_bytes, require, strict_json, timestamp)
import sys
sys.path.insert(0,str(ROOT/'sdk/python'))
from omniweft_sdk import (ApiError, CreateCube, EntityHandle, OutcomeUnknown, PolicyClient, PolicySession,
                         ProtocolError, RetryPolicyClient, RetryPolicySession, SetTransform, TemporaryTarget, Transform)
from omniweft_sdk import client as transport_module

PHASE='setup'
UUID='00000007-0000-4000-8000-000000000001'
def tx(n):return f'018f7242-4387-7c98-a114-{n:012x}'
def native_tx(p):return '018f7242-4387-7c98-a114-67787915a40'+str(p+1)
def snapshot(winner,revision):
    return {'format_version':1,'world_id':'workshop','seed':7,'max_slots':8,'world_revision':revision,
      'slots':[{'entity_uuid':UUID,'generation':1,'retired':False,'entity':{'prefab':'builtin.unit_cube',
        'authoring_revision':revision,'transform':{'position_m':[-3.0 if winner=='west' else 3.0,0.0,0.0],
        'rotation_xyzw':[0.0,0.0,0.0,1.0],'scale':[1.0,1.0,1.0]}}}]}
def receipt(identifier,committed,revision=1,create=True):
    return {'status':'committed' if committed else 'rejected','durability':'volatile','transaction_id':identifier,
      'world_revision':revision,'created':[{'temporary_id':'A','world_id':'workshop','entity_uuid':UUID,'generation':1}]
        if committed and create else [],'errors':[] if committed else [{'code':'REVISION_CONFLICT',
          'path':'/expected_world_revision','message':'Expected authoring revision is stale.'}]}
def receipt_dict(r):
    d=json.loads(json.dumps(asdict(r)))
    for e in d['errors']:
        if e['operation_index'] is None:del e['operation_index']
    return d

def expected_native(winner):
    west=winner=='west'
    receipts=[receipt(native_tx(0),west),receipt(native_tx(1),not west)]
    return {'schema_version':1,'example':'agents.contention','seed':7,'verified':True,'winner':winner,
      'receipts':receipts,'replayed_receipts':copy.deepcopy(receipts),'after_race':snapshot(winner,1),
      'after_compaction':snapshot(winner,5),'mismatch_code':'IDEMPOTENCY_MISMATCH','compacted_code':'REQUIRES_RESYNC',
      'next_sequences':[6,2] if west else [2,6],
      'usage':{'retained':[384,0] if west else [0,384],'working':[0,0],'requests':[0,0],
               'global_requests':0,'world_revision':5}}
def validate_native(value):
    require(type(value) is dict and value.get('winner') in ('west','east'),'native winner belongs to literal outcome set')
    exact(value,expected_native(value['winner']),'complete native contention fixture')

def desc(client):
    info=client._info
    return {'port':info.port,'token':info.token,'epoch':info.epoch}
def ops(principal):
    return [CreateCube('A'),SetTransform(TemporaryTarget('A'),Transform(position_m=(-3 if principal=='west' else 3,0,0)))]
def moved(principal):return [SetTransform(EntityHandle('workshop',UUID,1),Transform(position_m=(-3 if principal=='west' else 3,0,0)))]
def api_error(call,code):
    try:call()
    except ApiError as e:exact(e.code,code,'explicit retry rejection');return
    raise Failure('expected retry rejection missing')
def sdk_error(call,kind):
    try:call()
    except kind:return
    raise Failure('expected SDK rejection missing')
def raw(descriptor,body):return exchange(descriptor,request_bytes(descriptor,'POST','/v0/transactions',encoded(body)))
def headers_only(request):return request.split(b'\r\n\r\n')[0]+b'\r\n\r\n'
def rejected(value,code,status=409,path='/idempotency/sequence'):
    exact(value,(status,{'protocol_version':'0.1','status':'rejected','error':{'code':code,'path':path}}),'exact HTTP retry rejection during '+PHASE+' for '+code)
def idle(client):
    end=time.monotonic()+3
    while time.monotonic()<end:
        s=client.policy_status()
        if s.usage.global_requests==0:return s
        time.sleep(.005)
    raise Failure('policy lease did not release')

def settled(client,prepared):
    idle(client)
    return client.submit(prepared)
def observed(client):
    idle(client)
    return client.observe()

class Evidence(PriorEvidence):
    def __init__(self,directory,executable):
        super().__init__(directory,executable)
        for name in ('contention_oracle.py','contention_oracle_test.py','retry_native_test.cpp','retry_sdk_test.py'):
            p=ROOT/'tests'/name;self.sources[p.relative_to(ROOT).as_posix()]=digest(p.read_bytes())
        for name in ('retry-1.schema.json','control-0.1.schema.json'):
            p=ROOT/'schemas'/name;self.sources[p.relative_to(ROOT).as_posix()]=digest(p.read_bytes())
        self.manifest.update(work_item='PR-011',example='agents.contention',source_sha256=self.sources,
          limitations=['Opt-in policy.retry.v1 only; legacy profiles remain resync_only.',
                       'Volatile four-receipt window per principal; restart requires resync, not crash-safe exactly-once.',
                       'Retry payload/receipt field caps are host metadata bounds, not policy world quota or global RSS.',
                       'CPU evidence only; no GPU, physics or persistence claim.'])

@contextmanager
def session(executable,evidence):
    s=RetryPolicySession(executable)
    record={'command':['<contention_host>','--world','workshop','--seed','7','--max-slots','8',
            '--session-ttl-ms','30000','--max-runtime-ms','60000','--max-requests','1024'],
            'started_at':timestamp(),'exit_code':None}
    evidence.manifest['commands'].append(record)
    try:
        with s:
            for p in ('west','east'):evidence.register(desc(s.client(p)))
            yield s
    finally:
        record.update(exit_code=s.returncode,finished_at=timestamp())
    require(s.returncode==0,'retry host shuts down cleanly')

def transport_cases(executable,evidence):
    global PHASE
    PHASE='race'
    with session(executable,evidence) as s:
        clients=[s.client('west'),s.client('east')]
        for c in clients:
            caps=c.capabilities()
            exact(asdict(caps.retry_limits),{'receipts_per_principal':4,'canonical_payload_bytes':16384,
                'receipt_field_bytes':8192,'receipt_ttl_ms':2000,'principal_count':2},'negotiated exact metadata limits')
            sdk_error(lambda:PolicyClient(c._info,c._principal).capabilities(),ProtocolError)
        prepared=[c.prepare(ops(p),0,transaction_id=tx(i+1)) for i,(c,p) in enumerate(zip(clients,('west','east')))]
        require(all(c._info.epoch not in repr(p) and c._info.token not in repr(p) for c,p in zip(clients,prepared)),
                'prepared request repr excludes credentials')
        sdk_error(lambda:clients[1].submit(prepared[0]),ProtocolError)
        idle(clients[0])
        barrier=threading.Barrier(2);results=[None,None];failures=[]
        def run(i):
            try:barrier.wait(timeout=3);results[i]=clients[i].submit(prepared[i])
            except Exception:failures.append(True)
        threads=[threading.Thread(target=run,args=(i,)) for i in range(2)]
        for t in threads:t.start()
        for t in threads:t.join(timeout=5)
        require(not failures and not any(t.is_alive() for t in threads),'concurrent real SDK calls complete')
        require(all(x is not None for x in results),'both principals obtain receipts')
        winner=0 if results[0].status=='committed' else 1;name=('west','east')[winner]
        actual=[receipt_dict(r) for r in results]
        exact(actual,[receipt(tx(i+1),i==winner) for i in range(2)],'one commit and one complete explicit conflict')
        for c,p,r in zip(clients,prepared,results):exact(settled(c,p),r,'real committed/rejected receipt replay')
        first=observed(clients[1]).to_dict();exact(first,snapshot(name,1),'exact world after real concurrent proposals')
        for c in clients:idle(c)
        body=json.loads(prepared[winner]._body);d=desc(clients[winner])
        for field in ('transaction_id','expected_world_revision','budget','operations'):
            changed=copy.deepcopy(body)
            if field=='transaction_id':changed[field]=tx(99)
            elif field=='expected_world_revision':changed[field]=1
            elif field=='budget':changed[field]['max_operations']=3
            else:changed[field][1]['position_m'][0]=(-4 if winner==0 else 4)
            idle(clients[winner])
            rejected(raw(d,changed),'IDEMPOTENCY_MISMATCH')
        # JSON formatting/member order do not alter canonical typed equality.
        reformatted=json.dumps(body,indent=1,sort_keys=True).encode('ascii')
        idle(clients[winner])
        response=exchange(d,request_bytes(d,'POST','/v0/transactions',reformatted))
        require(response is not None and response[0]==200,'canonical payload ignores lexical JSON formatting')
        exact(response[1]['receipt'],actual[winner],'canonical replay retains exact receipt')
        for sequence in range(2,6):
            request=clients[winner].prepare(moved(name),sequence-1,transaction_id=tx(10+sequence))
            r=settled(clients[winner],request)
            if sequence==2:second_request,second_receipt=request,r
            exact(receipt_dict(r),receipt(tx(10+sequence),True,sequence,False),'fresh sequence after conflict')
            if sequence==2:exact(settled(clients[winner],prepared[winner]),results[winner],'SDK old receipt replay preserves current outer sequence')
        exact(settled(clients[winner],second_request),second_receipt,'old retained SDK receipt after full window of admissions')
        exact(clients[winner].policy_status().next_sequence,6,'SDK replay cannot roll current sequence backward')
        exact(observed(clients[1]).to_dict(),snapshot(name,5),'old retained SDK replay preserves complete world')
        api_error(lambda:settled(clients[winner],prepared[winner]),'REQUIRES_RESYNC')
        sdk_error(lambda:clients[winner].prepare(moved(name),5),OutcomeUnknown)
        clients[winner].policy_status();clients[winner].capabilities()
        sdk_error(lambda:clients[winner].prepare(moved(name),5),OutcomeUnknown)
        final=observed(clients[1]).to_dict();exact(final,snapshot(name,5),'compaction cannot reapply old create')
        clients[winner].resync_policy()
        idle(clients[winner])
        recovered=clients[winner].transact(moved(name),5,transaction_id=tx(20))
        exact(receipt_dict(recovered),receipt(tx(20),True,6,False),'explicit compaction resync recovers')
        usage=[asdict(idle(c).usage) for c in clients]
        exact(usage,[{'retained_bytes':384 if i==winner else 0,'working_bytes':0,'requests':0,'global_requests':0} for i in range(2)],'replays preserve world quota and refund live leases')
        evidence.retain_json('transport/race.json',{'winner':name,'receipts':actual,'after_race':first,'after_compaction':final,'usage':usage})

    PHASE='expiry'
    with session(executable,evidence) as s:
        east=s.client('east');prepared=east.prepare(ops('east'),0,transaction_id=tx(30))
        original=settled(east,prepared)
        time.sleep(1.1);exact(settled(east,prepared),original,'replay inside original TTL')
        time.sleep(1.05);api_error(lambda:settled(east,prepared),'REQUIRES_RESYNC')
        exact(observed(east).to_dict(),snapshot('east',1),'expired create is never reapplied')
        sdk_error(lambda:east.prepare(moved('east'),1),OutcomeUnknown)
        east.resync_policy();fresh=east.transact(moved('east'),1,transaction_id=tx(31))
        exact(receipt_dict(fresh),receipt(tx(31),True,2,False),'fresh sequence after expiry reconciliation')
        evidence.retain_json('transport/expiry.json',{'original':receipt_dict(original),'recovered':receipt_dict(fresh),
            'after':observed(east).to_dict(),'expired_code':'REQUIRES_RESYNC'})

    PHASE='delivery_loss'
    with session(executable,evidence) as s:
        east=s.client('east');prepared=east.prepare(ops('east'),0,transaction_id=tx(40))
        class LostResponse(transport_module._BoundedResponse):
            def read(self,*args,**kwargs):
                super().read(*args,**kwargs)
                raise OSError('fixture transport delivery failure')
        # Actual host commits; client response delivery fails at the transport boundary.
        with patch.object(transport_module,'_BoundedResponse',LostResponse):
            sdk_error(lambda:east.submit(prepared),OutcomeUnknown)
        require(east._uncertain,'transport failure records unknown outcome')
        sdk_error(lambda:east.prepare(ops('east'),0),OutcomeUnknown)
        observer=s.client('east');exact(observed(observer).to_dict(),snapshot('east',1),'failed delivery occurred after actual commit')
        recovered=settled(east,prepared)
        exact(receipt_dict(recovered),receipt(tx(40),True),'same prepared request recovers lost receipt')
        exact(observed(east).to_dict(),snapshot('east',1),'lost response recovery never duplicates effect')
        # Deliver another real request and discard its response bytes entirely.
        dropped=east.prepare(moved('east'),1,transaction_id=tx(41));d=desc(east)
        idle(east)
        sock=socket.create_connection(('127.0.0.1',d['port']),timeout=3)
        try:
            sock.sendall(request_bytes(d,'POST','/v0/transactions',dropped._body))
            end=time.monotonic()+2
            while observer.policy_status().world_revision!=2:
                require(time.monotonic()<end,'dropped-response request commits');time.sleep(.005)
        finally:sock.close()
        idle(observer);result=settled(east,dropped)
        exact(receipt_dict(result),receipt(tx(41),True,2,False),'unread real socket response recovered without reapply')
        exact(observed(observer).to_dict(),snapshot('east',2),'socket response loss exact world')
        evidence.retain_json('transport/lost-response.json',{'recovered':receipt_dict(recovered),'unread_recovered':receipt_dict(result),'after':observed(observer).to_dict()})
        PHASE='rotation'
        old=desc(east);old_body=json.loads(dropped._body)
        s.renew();new=s.client('east');evidence.register(desc(new));evidence.register(desc(s.client('west')))
        rejected(exchange(old,headers_only(request_bytes(old,'POST','/v0/transactions',encoded(old_body)))),'NOT_AUTHORIZED',401,'')
        rejected(raw(desc(new),old_body),'REQUIRES_RESYNC',409,'/idempotency/epoch')
        sdk_error(lambda:new.submit(dropped),ProtocolError)
        state=new.resync_policy();exact((state.next_sequence,state.world_revision),(1,2),'new epoch preserves state and resets only admission')
        idle(new)
        r=new.transact(moved('east'),2,transaction_id=tx(42))
        exact(receipt_dict(r),receipt(tx(42),True,3,False),'new epoch fresh proposal recovers')
        evidence.retain_json('transport/rotation.json',{'recovered':receipt_dict(r),'after':observed(new).to_dict(),'old_token':'NOT_AUTHORIZED','old_epoch':'REQUIRES_RESYNC'})

    PHASE='admission'
    with session(executable,evidence) as s:
        west,east=s.client('west'),s.client('east');p=west.prepare(ops('west'),0,transaction_id=tx(50));d=desc(west)
        body=json.loads(p._body);gap=copy.deepcopy(body);gap['idempotency']['sequence']=2
        rejected(raw(d,gap),'REQUIRES_RESYNC')
        # Authenticated other principal cannot reuse a foreign epoch.
        idle(west)
        rejected(raw(desc(east),body),'REQUIRES_RESYNC',409,'/idempotency/epoch')
        unsupported=copy.deepcopy(body);unsupported['operations']=[{'type':'entity.tags.set','target':{'temporary_id':'A'},'tags':['red']}]
        idle(west)
        rejected(raw(d,unsupported),'UNSUPPORTED_OPERATION',405,'/operations')
        idle(west)
        read=encoded({'protocol_version':'0.1','world_id':'workshop'})
        headers=headers_only(request_bytes(d,'POST','/v0/observe',read))
        excess=headers_only(request_bytes(d,'POST','/v0/transactions',encoded(body)))
        held_start=time.monotonic()
        sock=socket.create_connection(('127.0.0.1',d['port']),timeout=3)
        try:
            sock.sendall(headers);end=held_start+.5
            while west.policy_status().usage.requests!=1:
                require(time.monotonic()<end,'slow body holds existing principal lease');time.sleep(.005)
            # A status snapshot cannot extend the holder's original 1,000 ms deadline.
            require(time.monotonic()<end,'saturation probe starts with holder deadline margin')
            rejected(exchange(d,excess,half_close=True),'BUDGET_EXCEEDED',413,'/queue')
        finally:sock.close()
        idle(west);exact(west.policy_status().next_sequence,1,'gaps unsupported and saturated requests consume no sequence')
        r=settled(west,p);exact(receipt_dict(r),receipt(tx(50),True),'same-key recovery after preadmission denials')
        evidence.retain_json('transport/admission.json',{'gap':'REQUIRES_RESYNC','foreign_epoch':'REQUIRES_RESYNC','unsupported':'UNSUPPORTED_OPERATION','saturation':'BUDGET_EXCEEDED','recovered':receipt_dict(r),'after':observed(east).to_dict()})


def main():
    parser=argparse.ArgumentParser();parser.add_argument('--executable',required=True,type=Path)
    parser.add_argument('--host',required=True,type=Path);parser.add_argument('--native-test',type=Path);parser.add_argument('--evidence',type=Path)
    args=parser.parse_args();evidence=Evidence(args.evidence,args.executable)
    try:
        evidence.bind_executable('example',args.executable);evidence.bind_executable('host',args.host)
        if args.native_test:evidence.bind_executable('native_test',args.native_test)
        with tempfile.TemporaryDirectory(prefix='ow-contention-') as temp:
            output=Path(temp)/'example';tail=['--example','agents.contention','--headless','--seed','7','--verify','--output']
            run=evidence.run([str(args.executable),*tail,str(output)],['<examples>',*tail,'<output>'])
            require(run.returncode==0,'native contention example executes')
            actual=strict_json((output/'result.json').read_bytes());validate_native(actual)
            evidence.retain_json('native/actual.json',actual);evidence.retain_json('native/expected.json',expected_native(actual['winner']))
        if args.native_test:
            result=evidence.run([str(args.native_test)],['<retry_native_test>'])
            require(result.returncode==0,'retry native assertions execute')
            summary=strict_json(result.stdout.encode('utf-8'))
            require(type(summary) is dict and set(summary)=={'status','assertions'} and summary['status']=='passed'
                    and type(summary['assertions']) is int and summary['assertions']>=90,'bounded native result summary')
            evidence.retain_json('native/assertions.json',summary)
        sdk=evidence.run([sys.executable,str(ROOT/'tests/retry_sdk_test.py')],['<python>','tests/retry_sdk_test.py'])
        match=re.fullmatch(r'retry SDK tests passed: ([0-9]+) assertions\s*',sdk.stdout)
        require(sdk.returncode==0 and match is not None and int(match.group(1))>=53,'strict retry SDK assertions execute')
        evidence.retain_json('native/sdk-assertions.json',{'status':'passed','assertions':int(match.group(1))})
        transport_cases(args.host,evidence)
        evidence.finish('passed');print('contention oracle passed: '+str(len(evidence.manifest['assertions']))+' assertions');return 0
    except Failure as error:evidence.finish('failed',str(error));raise
    except ApiError as error:
        message='unexpected API rejection during '+PHASE+': '+error.code
        evidence.finish('failed',message);raise Failure(message) from None
    except OutcomeUnknown:
        message='unexpected uncertain response during '+PHASE
        evidence.finish('failed',message);raise Failure(message) from None
    except Exception:evidence.finish('failed','unexpected private internal error');raise

if __name__=='__main__':raise SystemExit(main_guard(main))
