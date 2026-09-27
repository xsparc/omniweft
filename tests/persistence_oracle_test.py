#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
import copy
from persistence_oracle import expected,validate,validate_crashes,state,result,PHASES,NEGATIVES,validate_negatives,Failure,CHECKS

def main():
    baseline=expected();validate(baseline)
    crashes={'schema_version':1,'cases':[]}
    for phase in PHASES:
        compact='checkpoint' in phase or 'reclaim' in phase;n=5 if compact else (1 if phase=='before_blob_flush' else 2);target=5 if compact else 2
        crashes['cases'].append({'phase':phase,'exit_code':86,'acknowledgement':result(2,2) if phase=='after_ack' else None,
          'epoch_preserved':True,'evicted':result(5,code='REQUIRES_RESYNC') if compact else None,
          'recovered':state(n),'lookup':result(n,n,True),'retry':result(target,target,n==target),'after_retry':state(target)})
    validate_crashes(crashes);cases=[]
    for path in [('verified',),('epoch_preserved',),('states',1,'snapshot','slots',0,'generation'),
      ('states',4,'canonical_hex'),('receipts',1,'receipt','durability'),('retry','replayed'),
      ('mismatch','code'),('expired','code'),('recovered','high_water'),('recovered','low_water'),
      ('lookup','receipt','created'),('compacted','recovery_required')]:
        v=copy.deepcopy(baseline);node=v
        for k in path[:-1]:node=node[k]
        old=node[path[-1]];node[path[-1]]=not old if type(old) is bool else (old+1 if type(old) is int else 'corrupt')
        cases.append((validate,v))
    for index,key,value in [(0,'exit_code',0),(1,'phase','missing'),(2,'recovered',state(1)),(3,'lookup',result(2)),
      (4,'retry',result(2,2,False)),(5,'after_retry',state(3)),(4,'acknowledgement',None),(3,'acknowledgement',result(2,2))]:
        v=copy.deepcopy(crashes);v['cases'][index][key]=value;cases.append((validate_crashes,v))
    v=copy.deepcopy(baseline);v['epoch']='private';cases.append((validate,v))
    v=copy.deepcopy(crashes);v['cases'][0]['recovered']['recovery_required']=0;cases.append((validate_crashes,v))
    negatives={'schema_version':1,'cases':[{'case':p,'preserved':True,'code':'RECOVERED_PREFIX' if p=='truncated-tail' else 'CORRUPT_STORE','recovery':state(1 if p=='truncated-tail' else 2),'continued':state(2)} for p in NEGATIVES]}
    validate_negatives(negatives)
    for index in range(len(NEGATIVES)):
        v=copy.deepcopy(negatives);v['cases'][index]['preserved']=False;cases.append((validate_negatives,v))
    v=copy.deepcopy(negatives);v['cases'][0]['recovery']=state(2);cases.append((validate_negatives,v))
    v=copy.deepcopy(crashes);v['cases'][0]['epoch_preserved']=False;cases.append((validate_crashes,v))
    v=copy.deepcopy(crashes);v['cases'][5]['evicted']=result(5,1,True);cases.append((validate_crashes,v))
    for validator,value in cases:
        try:validator(value)
        except Failure:pass
        else:raise RuntimeError('corrupt persistence evidence accepted')
    CHECKS.clear();print('persistence oracle selftest passed: '+str(len(cases))+' corruptions');return 0
if __name__=='__main__':raise SystemExit(main())
