#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Strict negotiation and local SDK state tests; engine proof lives in contention_oracle."""
import copy
import json
import sys
from dataclasses import FrozenInstanceError
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'sdk/python'))
from omniweft_sdk import (Capabilities, ConnectionInfo, CreateCube, OutcomeUnknown, ProtocolError,
                         RetryCapabilities, RetryPolicyClient)

CAPS={'protocol_version':'0.1','epoch':'b'*64,'next_sequence':1,'world_id':'workshop','world_revision':0,
 'operations':['entity.create','transform.set','entity.delete'],
 'limits':{'max_header_bytes':16384,'max_body_bytes':1048576,'max_response_bytes':4194304,
           'max_operations':256,'max_slots':8,'request_timeout_ms':1000,'session_ttl_ms':30000},
 'admission':'synchronous','durability':'volatile','retry_mode':'retained_receipts_v1',
 'retry_limits':{'receipts_per_principal':4,'canonical_payload_bytes':16384,'receipt_field_bytes':8192,
                 'receipt_ttl_ms':2000,'principal_count':2}}
checks=0
def check(v):
    global checks
    checks+=1
    if not v:raise RuntimeError('retry SDK invariant failed')
def rejected(call,kind=ProtocolError):
    try:call()
    except kind:check(True);return
    raise RuntimeError('retry SDK accepted invalid state')
def main():
    schema=json.loads((Path(__file__).resolve().parents[1]/'schemas/retry-1.schema.json').read_text(encoding='utf-8'))
    declared=schema['$defs']['Capabilities']['properties']
    check(set(declared)==set(CAPS))
    check(declared['retry_mode']=={'const':'retained_receipts_v1'})
    check(declared['retry_limits']['properties']=={k:{'type':'integer','const':v} for k,v in CAPS['retry_limits'].items()})
    caps=RetryCapabilities.from_dict(copy.deepcopy(CAPS));check(caps.retry_mode=='retained_receipts_v1')
    rejected(lambda:Capabilities.from_dict(CAPS))
    legacy=copy.deepcopy(CAPS);del legacy['retry_limits'];legacy['retry_mode']='resync_only'
    Capabilities.from_dict(legacy);rejected(lambda:RetryCapabilities.from_dict(legacy))
    for name in CAPS['retry_limits']:
        for value in (True,0,-1,'4',CAPS['retry_limits'][name]+1):
            bad=copy.deepcopy(CAPS);bad['retry_limits'][name]=value
            rejected(lambda:RetryCapabilities.from_dict(bad))
    for mode in ('resync_only','retained_receipts_v2',True,None):
        bad=copy.deepcopy(CAPS);bad['retry_mode']=mode;rejected(lambda:RetryCapabilities.from_dict(bad))
    bad=copy.deepcopy(CAPS);bad['retry_limits']['extra']=1;rejected(lambda:RetryCapabilities.from_dict(bad))
    info=ConnectionInfo('127.0.0.1',12345,'a'*64,'b'*64,30000)
    client=RetryPolicyClient(info,'east');other=RetryPolicyClient(info,'west')
    responses=[];sent=[]
    def request(method,route,body=None,**kwargs):
        if route=='/v0/capabilities':return copy.deepcopy(CAPS)
        sent.append(copy.deepcopy(body));return responses.pop(0)
    client._request=request
    operations=[CreateCube('A')];prepared=client.prepare(operations,0,transaction_id='018f7242-4387-7c98-a114-000000000101')
    operations.clear();check(json.loads(prepared._body)['operations'][0]['temporary_id']=='A')
    rejected(lambda:setattr(prepared,'sequence',2),FrozenInstanceError)
    rejected(lambda:other.submit(prepared))
    rejected(lambda:client.prepare([CreateCube('B')],0),OutcomeUnknown)
    check(info.token not in repr(prepared) and info.epoch not in repr(prepared))
    result={'protocol_version':'0.1','epoch':info.epoch,'next_sequence':2,
      'receipt':{'status':'committed','durability':'volatile','transaction_id':prepared.transaction_id,
                 'world_revision':1,'created':[],'errors':[]}}
    wrong=copy.deepcopy(result);wrong['receipt']['transaction_id']='018f7242-4387-7c98-a114-000000000102'
    responses.append(wrong);rejected(lambda:client.submit(prepared),OutcomeUnknown)
    check(client._uncertain and client._pending is prepared)
    client.capabilities();rejected(lambda:client.prepare([CreateCube('B')],1),OutcomeUnknown)
    responses.append(result);receipt=client.submit(prepared);check(not client._uncertain and client._pending is None)
    check(sent[0]==sent[1]==json.loads(prepared._body))
    newer=copy.deepcopy(result);newer['next_sequence']=4;responses.append(newer)
    check(client.submit(prepared)==receipt and client._next==4)
    backwards=copy.deepcopy(result);responses.append(backwards)
    rejected(lambda:client.submit(prepared),OutcomeUnknown);check(client._next==4)
    for key in ('epoch','next_sequence','receipt'):
        bad=copy.deepcopy(newer);del bad[key];responses.append(bad)
        rejected(lambda:client.submit(prepared),OutcomeUnknown)
    responses.append(newer);check(client.submit(prepared)==receipt and client._pending is None)
    print('retry SDK tests passed: '+str(checks)+' assertions');return 0
if __name__=='__main__':raise SystemExit(main())
