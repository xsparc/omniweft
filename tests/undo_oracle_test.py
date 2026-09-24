#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
import copy
from undo_oracle import expected,validate,Failure,CHECKS


def main():
    value=expected();validate(value);cases=[]
    for path in [('clear_preserved_world',),('seed',),('records',6,'after','world_revision'),
      ('records',6,'after','slots',0,'entity','authoring_revision'),('records',6,'after','slots',0,'generation'),
      ('records',6,'after','slots',0,'entity','transform','position_m',0),('records',6,'canonical_hex'),
      ('records',12,'receipt','errors',0,'code'),('records',12,'history','expected_revision'),
      ('records',14,'after','slots',1,'entity','tags',0),('records',7,'history','cursor'),
      ('records',0,'receipt','created',0,'entity_uuid')]:
        bad=copy.deepcopy(value);node=bad
        for key in path[:-1]:node=node[key]
        v=node[path[-1]];node[path[-1]]=False if type(v) is bool else (v+1 if type(v) in (int,float) else 'corrupt');cases.append(bad)
    bad=copy.deepcopy(value);bad['records'][0]['after']['slots'][0]['retired']=0;cases.append(bad)
    bad=copy.deepcopy(value);bad['secret']='extra';cases.append(bad)
    for bad in cases:
        try:validate(bad)
        except Failure:pass
        else:raise RuntimeError('corrupt undo proof accepted')
    CHECKS.clear();print('undo oracle selftest passed: '+str(len(cases))+' corruptions');return 0
if __name__=='__main__':raise SystemExit(main())
