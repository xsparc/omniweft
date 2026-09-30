#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
import copy
from gltf_oracle import expected, validate, Failure, CHECKS


def main():
    value = expected(); validate(value); cases = []
    paths = [('seed',), ('verified',), ('detached_output_unchanged',), ('file_hex',), ('file_sha256',)]
    for i in range(4):
        paths += [('steps', i, 'receipt', 'revision'), ('steps', i, 'snapshot', 'revision'),
                  ('steps', i, 'scene', 'source_asset', 'id'), ('steps', i, 'scene', 'source_asset', 'manifest', 'source'),
                  ('steps', i, 'scene', 'source_asset', 'manifest', 'license'), ('steps', i, 'scene', 'profile'),
                  ('steps', i, 'scene', 'meshes', 0, 'primitives', 0, 'positions', 0, 0),
                  ('steps', i, 'scene', 'meshes', 0, 'primitives', 0, 'indices', 2),
                  ('steps', i, 'scene', 'meshes', 0, 'primitives', 0, 'material'),
                  ('steps', i, 'scene', 'meshes', 0, 'primitives', 1, 'material'),
                  ('steps', i, 'scene', 'nodes', 1, 'world_matrix', 12),
                  ('steps', i, 'scene', 'nodes', 1, 'local_matrix', 0),
                  ('steps', i, 'scene', 'nodes', 2, 'world_bounds', 'minimum', 0),
                  ('steps', i, 'scene', 'materials', 0, 'base_color_factor', 0),
                  ('steps', i, 'scene', 'world_bounds', 'maximum', 1)]
    for i in range(12):
        paths += [('negative', i, 'receipt', 'code'), ('negative', i, 'after', 'revision'),
                  ('negative', i, 'input_hex'), ('negative', i, 'input_unchanged'), ('negative', i, 'scene')]
    for path in paths:
        bad = copy.deepcopy(value); node = bad
        for key in path[:-1]: node = node[key]
        v = node[path[-1]]
        node[path[-1]] = not v if type(v) is bool else v+1 if type(v) in (int,float) else 'corrupt'
        cases.append(bad)
    bad = copy.deepcopy(value); bad['steps'][0]['scene']['nodes'][1]['world_matrix'][0] = True; cases.append(bad)
    bad = copy.deepcopy(value); bad['steps'][0]['snapshot']['current_roots'] = ['0'*64]; cases.append(bad)
    bad = copy.deepcopy(value); bad['steps'][0]['scene']['materials'][0]['metallic_factor'] = float('nan'); cases.append(bad)
    bad = copy.deepcopy(value); bad['unexpected'] = 'private-data'; cases.append(bad)
    bad = copy.deepcopy(value); bad['steps'][0]['scene']['meshes'][0]['primitives'].reverse(); cases.append(bad)
    for bad in cases:
        try: validate(bad)
        except Failure: pass
        else: raise RuntimeError('corrupt GLB import proof accepted')
    CHECKS.clear(); print('gltf oracle selftest passed: ' + str(len(cases)) + ' corruptions'); return 0


if __name__ == '__main__': raise SystemExit(main())
