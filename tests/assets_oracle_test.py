#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
import copy
from assets_oracle import expected, validate, Failure, CHECKS


def main():
    value = expected(); validate(value); cases = []
    paths = [('seed',), ('verified',), ('detached_query_unchanged',),
             ('roundtrip', 'bundle_hex'), ('roundtrip', 'file_sha256'), ('roundtrip', 'export_hex'),
             ('roundtrip', 'snapshot', 'revision'), ('roundtrip', 'snapshot', 'assets', 0, 'id'),
             ('roundtrip', 'snapshot', 'assets', 0, 'manifest', 'source'),
             ('roundtrip', 'snapshot', 'assets', 0, 'manifest', 'license'),
             ('roundtrip', 'snapshot', 'assets', 0, 'manifest', 'content_hash'),
             ('roundtrip', 'snapshot', 'blobs', 0, 'bytes_hex'), ('rle', 'input_hex'), ('rle', 'export_hex'),
             ('steps', 1, 'receipt', 'revision'), ('steps', 6, 'snapshot', 'history_roots', 0, 'asset_ids', 0),
             ('steps', 8, 'receipt', 'blobs_removed'), ('steps', 10, 'receipt', 'manifests_removed'),
             ('steps', 13, 'snapshot', 'current_roots', 0)]
    for i in range(16):
        paths.extend([('negative', i, 'receipt', 'code'), ('negative', i, 'after', 'revision')])
    paths.extend([('negative', 0, 'input_unchanged'), ('negative', 4, 'input_hex')])
    for path in paths:
        bad = copy.deepcopy(value); node = bad
        for key in path[:-1]: node = node[key]
        v = node[path[-1]]
        node[path[-1]] = not v if type(v) is bool else v + 1 if type(v) is int else 'corrupt'
        cases.append(bad)
    bad = copy.deepcopy(value); bad['steps'][0]['snapshot']['revision'] = True; cases.append(bad)
    bad = copy.deepcopy(value); bad['roundtrip']['snapshot']['current_roots'] = ['0' * 64]; cases.append(bad)
    bad = copy.deepcopy(value); bad['unexpected'] = 'private-data'; cases.append(bad)
    for bad in cases:
        try: validate(bad)
        except Failure: pass
        else: raise RuntimeError('corrupt asset proof accepted')
    CHECKS.clear(); print('assets oracle selftest passed: ' + str(len(cases)) + ' corruptions'); return 0


if __name__ == '__main__': raise SystemExit(main())
