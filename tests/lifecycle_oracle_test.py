#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Corrupt independently constructed reports; this is not runtime evidence."""
import copy
from lifecycle_oracle import (CHECKS, Failure, expected_status, policy, receipt, state,
                              tx, validate, validate_transport)


def runtime(tick,revision=0,x=3):
    return {'schema_version':1,'tick_rate_hz':60,'max_catch_up_steps':4,'simulation_tick':tick,
      'snapshot_sequence':tick+1,'overload_count':0,'dropped_ticks':0,'remainder_units':0,
      'snapshot':state(revision,x),'presentation':{'enabled':False,'ready':False,'frame_count':0,
      'world_revision':0,'snapshot_sequence':0}}


def example():
    rows=[]
    for i in range(1,7):
        progress=None
        if i<=4:
            pending={**expected_status(i),'state':'running','error_code':None,'late_results':0,'worker_cleaned':False}
            progress={'statuses':[pending,copy.deepcopy(pending)],'runtime':[runtime(1),runtime(2)]}
        rows.append({'phase':('timeout','cancelled','crashed','superseded','replacement','restarted')[i-1],
          'status':expected_status(i),'snapshot':state(max(0,i-4),4 if i==6 else 3),
          'policy':policy(max(0,i-4)),'progress':progress})
    return {'schema_version':1,'example':'agents.worker_failure','seed':7,'lane':'cpu',
      'records':rows,'retained':[expected_status(i) for i in range(1,7)],'old_submit_rejected':True,
      'restart_generations':[2,4],'host_exit_code':0}


def transport():
    rows=[]
    for i in (1,2):
        base={'request_id':i,'generation':i,'state':'submitting','expected_revision':i-1,
          'late_results':0,'receipt':None,'error_code':None,'worker_cleaned':True}
        rows.append({'submitting':base,'cancel_result':'too_late','replacement_blocked':True,
          'runtime':[runtime(10,i,i+2),runtime(11,i,i+2)],
          'unknown':{**base,'state':'outcome_unknown','error_code':'OUTCOME_UNKNOWN'}})
    expired={**rows[1]['unknown'],'error_code':'REQUIRES_RESYNC'}
    return {'first':rows[0],'restarted_generation':2,'after_loss':state(1),
      'recovered':{**rows[0]['submitting'],'state':'committed','receipt':receipt(tx(101),1)},
      'after_retry':state(1),'second':rows[1],'expired':expired,'after_expiry':state(2,4),
      'reconciled':{**expired,'state':'reconciled','error_code':None},
      'fresh':{**rows[1]['submitting'],'request_id':3,'expected_revision':2,'state':'committed','receipt':receipt(tx(103),3)},
      'final':state(3,5),'policy':policy(3)}


def main():
    count=0
    for original,verify,paths in [
      (example(),validate,[('seed',),('host_exit_code',),('old_submit_rejected',),('restart_generations',0),
        ('records',0,'status','state'),('records',1,'status','late_results'),('records',2,'status','error_code'),
        ('records',3,'status','generation'),('records',4,'status','receipt','world_revision'),
        ('records',4,'status','receipt','created',0,'generation'),('records',5,'snapshot','world_revision'),
        ('records',5,'snapshot','slots',0,'entity','transform','position_m',0),
        ('records',4,'policy','usage','retained_bytes'),('records',4,'policy','next_sequence'),
        ('records',0,'progress','runtime',1,'simulation_tick'),('records',0,'progress','statuses',0,'state'),
        ('retained',0,'worker_cleaned'),('retained',3,'late_results')]),
      (transport(),validate_transport,[('restarted_generation',),('first','cancel_result'),
        ('first','submitting','state'),('first','replacement_blocked'),('first','unknown','error_code'),
        ('first','runtime',1,'simulation_tick'),('after_loss','world_revision'),
        ('recovered','receipt','transaction_id'),('after_retry','slots',0,'generation'),
        ('second','unknown','expected_revision'),('expired','error_code'),('after_expiry','world_revision'),
        ('reconciled','state'),('fresh','receipt','world_revision'),('final','slots',0,'entity','transform','position_m',0),
        ('policy','next_sequence'),('policy','usage','requests')])]:
        verify(original)
        cases=[]
        for path in paths:
            bad=copy.deepcopy(original);node=bad
            for key in path[:-1]:node=node[key]
            old=node[path[-1]]
            node[path[-1]]=not old if type(old) is bool else 'corrupt' if type(old) is str else old+1
            cases.append(bad)
        bad=copy.deepcopy(original);bad['unexpected']='private data';cases.append(bad)
        for bad in cases:
            try:verify(bad)
            except Failure:count+=1
            else:raise RuntimeError('corrupted lifecycle evidence accepted')
    # Exact types cannot silently turn a counter into a boolean.
    bad=example();bad['records'][0]['status']['request_id']=True
    try:validate(bad)
    except Failure:count+=1
    else:raise RuntimeError('boolean identity accepted')
    CHECKS.clear();print('lifecycle oracle selftest passed: '+str(count)+' corruptions');return 0


if __name__=='__main__':raise SystemExit(main())
