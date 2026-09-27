#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
import copy
from replay_oracle import expected,validate,Failure,CHECKS

def main():
    value=expected();validate(value);cases=[]
    paths=[('seed',),('verified',),('rejected_not_recorded',),('log','assets',0,'content_sha256'),
      ('log','assets',0,'manifest'),('log','records',2,'created',0,'generation'),('log','records',3,'checkpoint_hex'),
      ('log','records',1,'sequence'),('log','records',2,'boundary_tick'),('log','records',2,'operations',0,'target','generation'),
      ('original_checkpoints',3,'slots',0,'generation'),('replayed_checkpoints',2,'slots',1,'entity','authoring_revision'),
      ('recovery_snapshot','slots',0,'entity','transform','position_m',0),('negative',0,'world_returned'),
      ('negative',1,'code'),('original_receipts',2,'created',0,'entity_uuid')]
    for path in paths:
        bad=copy.deepcopy(value);node=bad
        for key in path[:-1]:node=node[key]
        v=node[path[-1]];node[path[-1]]=not v if type(v) is bool else (v+1 if type(v) in (int,float) else 'corrupt');cases.append(bad)
    bad=copy.deepcopy(value);bad['original_checkpoints'][0]['slots'][0]['retired']=0;cases.append(bad)
    bad=copy.deepcopy(value);bad['log']['epoch']='extra';cases.append(bad)
    for bad in cases:
        try:validate(bad)
        except Failure:pass
        else:raise RuntimeError('corrupt replay proof accepted')
    CHECKS.clear();print('replay oracle selftest passed: '+str(len(cases))+' corruptions');return 0
if __name__=='__main__':raise SystemExit(main())
