#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
import copy
from contention_oracle import expected_native,validate_native,Failure,CHECKS

def main():
    cases=[]
    for winner in ('west','east'):
        original=expected_native(winner);validate_native(original)
        for path in [('winner',),('receipts',0,'world_revision'),('after_race','world_revision'),
                     ('after_compaction','slots',0,'generation'),('next_sequences',0),('mismatch_code',),
                     ('compacted_code',),('usage','retained',0),('replayed_receipts',1,'transaction_id')]:
            bad=copy.deepcopy(original);node=bad
            for key in path[:-1]:node=node[key]
            old=node[path[-1]];node[path[-1]]='corrupt' if type(old) is str else old+1;cases.append(bad)
        bad=copy.deepcopy(original);bad['seed']=True;cases.append(bad)
        bad=copy.deepcopy(original);bad['secret']='extra';cases.append(bad)
    for bad in cases:
        try:validate_native(bad)
        except Failure:pass
        else:raise RuntimeError('corrupted contention proof accepted')
    CHECKS.clear();print('contention oracle selftest passed: '+str(len(cases))+' corruptions');return 0
if __name__=='__main__':raise SystemExit(main())
