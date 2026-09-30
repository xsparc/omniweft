#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Independent GLB encoder, literal geometry/transform proof and retained evidence."""
import argparse
import copy
import json
import struct
import tempfile
from pathlib import Path
from assets_oracle import asset, snapshot, receipt
from hierarchy_oracle import Evidence as PriorEvidence
from sdk_test_support import ROOT, CHECKS, Failure, digest, exact, require, strict_json, main_guard


def document():
    return {
        'asset': {'version': '2.0', 'generator': 'Omniweft seed7 fixture'},
        'buffers': [{'byteLength': 82}],
        'bufferViews': [
            {'buffer': 0, 'byteOffset': 4, 'byteLength': 60, 'byteStride': 16, 'target': 34962},
            {'buffer': 0, 'byteOffset': 68, 'byteLength': 6, 'target': 34963},
            {'buffer': 0, 'byteOffset': 76, 'byteLength': 6, 'target': 34963}],
        'accessors': [
            {'bufferView': 0, 'componentType': 5126, 'count': 4, 'type': 'VEC3', 'min': [0,0,0], 'max': [2,1,0]},
            {'bufferView': 1, 'componentType': 5123, 'count': 3, 'type': 'SCALAR'},
            {'bufferView': 2, 'componentType': 5123, 'count': 3, 'type': 'SCALAR'}],
        'meshes': [{'primitives': [
            {'attributes': {'POSITION': 0}, 'indices': 1, 'material': 0, 'mode': 4},
            {'attributes': {'POSITION': 0}, 'indices': 2}]}],
        'materials': [{'name': 'seed7-blue', 'doubleSided': True, 'alphaMode': 'OPAQUE',
                       'pbrMetallicRoughness': {'baseColorFactor': [0.25,0.5,1,1], 'metallicFactor': 0, 'roughnessFactor': 0.75}}],
        'nodes': [{'translation': [7,2,1], 'scale': [2,2,2], 'children': [1]},
                  {'mesh': 0, 'translation': [1,0,0], 'rotation': [0,0,1,0], 'scale': [0.5,1,1]},
                  {'mesh': 0, 'translation': [-3,0,-1], 'scale': [-1,1,1]}],
        'scenes': [{'nodes': [0,2]}], 'scene': 0}


def binary():
    rows = [(0,0,0,777), (2,0,0,777), (0,1,0,777), (2,1,0,777)]
    return struct.pack('<I', 7) + b''.join(struct.pack('<4f', *p) for p in rows) + struct.pack('<8H', 0,1,2,0,1,3,2,0)


def glb(doc=None, bin_data=None):
    text = json.dumps(document() if doc is None else doc, sort_keys=True, separators=(',', ':'), ensure_ascii=True).encode('ascii')
    text += b' ' * (-len(text) % 4)
    data = binary() if bin_data is None else bin_data
    return (struct.pack('<III', 0x46546c67, 2, 28 + len(text) + len(data))
            + struct.pack('<II', len(text), 0x4e4f534a) + text + struct.pack('<II', len(data), 0x004e4942) + data)


def manifest(raw, source):
    return {'format_version': 1, 'content_hash': digest(raw), 'decoded_length': len(raw),
            'media_type': 'model/gltf-binary', 'source': source, 'license': 'Apache-2.0'}


def bounds(a, b):
    return {'minimum': list(map(float, a)), 'maximum': list(map(float, b))}


def matrix(x, y, z, tx, ty, tz):
    return list(map(float, [x,0,0,0, 0,y,0,0, 0,0,z,0, tx,ty,tz,1]))


def expected_scene(source='generated:seed7/quad'):
    positions = [[0.0,0.0,0.0], [2.0,0.0,0.0], [0.0,1.0,0.0], [2.0,1.0,0.0]]
    def node(mesh, children, translation, rotation, scale, local, world, box):
        return dict(mesh=mesh, children=children, translation=list(map(float, translation)), rotation=list(map(float, rotation)), scale=list(map(float, scale)),
                    local_matrix=local, world_matrix=world, world_bounds=box)
    return {'profile': 1, 'source_asset': asset(manifest(glb(), source)), 'roots': [0,2],
            'meshes': [{'primitives': [
                {'positions': positions, 'indices': [0,1,2], 'material': 0, 'bounds': bounds([0,0,0],[2,1,0])},
                {'positions': positions, 'indices': [1,3,2], 'material': None, 'bounds': bounds([0,0,0],[2,1,0])}]}],
            'nodes': [
                node(None, [1], [7,2,1], [0,0,0,1], [2,2,2], matrix(2,2,2,7,2,1), matrix(2,2,2,7,2,1), None),
                node(0, [], [1,0,0], [0,0,1,0], [0.5,1,1], matrix(-0.5,-1,1,1,0,0), matrix(-1,-2,2,9,2,1), bounds([7,0,1],[9,2,1])),
                node(0, [], [-3,0,-1], [0,0,0,1], [-1,1,1], matrix(-1,1,1,-3,0,-1), matrix(-1,1,1,-3,0,-1), bounds([-5,0,-1],[-3,1,-1]))],
            'materials': [{'name': 'seed7-blue', 'base_color_factor': [0.25,0.5,1.0,1.0], 'metallic_factor': 0.0, 'roughness_factor': 0.75, 'double_sided': True}],
            'world_bounds': bounds([-5,0,-1], [9,2,1])}


def negative_inputs():
    doc, data = document(), bytearray(binary())
    data[68] = 4
    result = [('invalid-index', glb(doc, data), 'INVALID_INDEX')]
    data = bytearray(binary()); data[4:8] = bytes.fromhex('0000c07f')
    result.append(('nonfinite-position', glb(doc, data), 'NONFINITE_VALUE'))
    d = copy.deepcopy(doc); d.update(extensionsUsed=['KHR_draco_mesh_compression'], extensionsRequired=['KHR_draco_mesh_compression'])
    result.extend([('required-extension', glb(d), 'UNSUPPORTED_REQUIRED_EXTENSION'),
                   ('file-budget', bytes(65537), 'BUDGET_EXCEEDED')])
    d = copy.deepcopy(doc); d['accessors'][0]['count'] = 257
    result.extend([('vertex-budget', glb(d), 'BUDGET_EXCEEDED'), ('stale-revision', glb(), 'REVISION_CONFLICT')])
    d = copy.deepcopy(doc); d['meshes'][0]['primitives'][0]['material'] = 4
    result.append(('material-reference', glb(d), 'INVALID_SCHEMA'))
    d = copy.deepcopy(doc); d['buffers'][0]['uri'] = 'fixture.bin'
    result.append(('external-uri', glb(d), 'UNSUPPORTED_PROFILE'))
    d = copy.deepcopy(doc); d['nodes'][1]['children'] = [0]
    result.append(('cycle', glb(d), 'INVALID_SCHEMA'))
    d = copy.deepcopy(doc); d['accessors'][0]['max'] = [3,1,0]
    result.append(('declared-bounds', glb(d), 'INVALID_SCHEMA'))
    data = bytearray(binary()); data[82] = 1
    result.extend([('bin-padding', glb(doc, data), 'INVALID_GLB'), ('truncated', glb()[:-1], 'INVALID_GLB')])
    return result


def expected():
    raw = glb(); a, b = [manifest(raw, 'generated:seed7/' + suffix) for suffix in ('quad', 'quad-alt')]
    aid, bid = asset(a)['id'], asset(b)['id']; steps = []
    for label, revision, m, ms in [('import', 1, a, [a]), ('deduplicate', 2, a, [a]),
                                   ('distinct-provenance', 3, b, [a,b]), ('recovery', 4, a, [a,b])]:
        steps.append({'case': label, 'receipt': receipt(revision, [asset(m)['id']]),
                      'scene': expected_scene(m['source']), 'snapshot': snapshot(revision, ms, [raw])})
    unchanged = snapshot(3, [a,b], [raw])
    require(aid != bid, 'independent fixture provenance identities differ')
    return {'schema_version': 1, 'example': 'meshes.import_gltf', 'seed': 7, 'verified': True,
            'file_hex': raw.hex(), 'file_sha256': digest(raw), 'detached_output_unchanged': True,
            'steps': steps, 'negative': [{'case': label, 'input_hex': data.hex(), 'input_unchanged': True,
                'receipt': receipt(3, code=code), 'scene': None, 'before': copy.deepcopy(unchanged),
                'after': copy.deepcopy(unchanged)} for label, data, code in negative_inputs()]}


def validate(value):
    exact(value, expected(), 'complete literal GLB import proof')
    raw = bytes.fromhex(value['file_hex'])
    exact(struct.unpack_from('<III', raw), (0x46546c67, 2, len(raw)), 'independent GLB header')
    json_size, chunk = struct.unpack_from('<II', raw, 12)
    exact(chunk, 0x4e4f534a, 'independent JSON chunk')
    exact(strict_json(raw[20:20+json_size]), document(), 'independent fixture document')
    bin_size, kind = struct.unpack_from('<II', raw, 20+json_size)
    exact((bin_size, kind), (84, 0x004e4942), 'independent BIN header')
    data = raw[28+json_size:]; exact(data, binary(), 'independent binary fixture')
    decoded = [list(struct.unpack_from('<3f', data, 4 + i*16)) for i in range(4)]
    exact(decoded, [[0.0,0.0,0.0],[2.0,0.0,0.0],[0.0,1.0,0.0],[2.0,1.0,0.0]], 'strided source positions')
    exact(list(struct.unpack_from('<3H', data, 68)), [0,1,2], 'first triangle')
    exact(list(struct.unpack_from('<3H', data, 76)), [1,3,2], 'second triangle')
    # Independent direct geometry formula, not production matrix composition.
    transformed = [[[9-x,2-2*y,1+2*z] for x,y,z in decoded], [[-3-x,y,-1+z] for x,y,z in decoded]]
    for step in value['steps']:
        for i, points in enumerate(transformed, 1):
            box = bounds([min(p[c] for p in points) for c in range(3)], [max(p[c] for p in points) for c in range(3)])
            exact(step['scene']['nodes'][i]['world_bounds'], box, 'bounds from independently transformed vertices')
        require(step['scene']['meshes'][0]['primitives'][1]['material'] is None, 'default material has no invented source index')


class Evidence(PriorEvidence):
    def __init__(self, directory, executable):
        super().__init__(directory, executable)
        for name in ('tests/gltf_oracle.py', 'tests/gltf_oracle_test.py', 'tests/gltf_native_test.cpp',
                     'tests/assets_oracle.py', '.github/workflows/native-quality.yml',
                     'cmake/ToolchainPolicy.cmake', 'tools/bootstrap.py'):
            self.sources[name] = digest((ROOT / name).read_bytes())
        self.manifest.update(work_item='PR-017', example='meshes.import_gltf', source_sha256=self.sources,
            limitations=['Bounded POSITION-only GLB/TRS profile; not a full glTF implementation.',
                         'Original source asset retained in volatile native Catalog; detached scene has no World authority.',
                         'No cooked format, renderer, textures, remote endpoint, persistence migration, GPU or physics evidence.',
                         'Exact seed fixture values do not imply general cross-platform bitwise floating-point equality.'])


def main():
    parser = argparse.ArgumentParser(); parser.add_argument('--executable', required=True, type=Path)
    parser.add_argument('--native-test', type=Path); parser.add_argument('--evidence', type=Path)
    args = parser.parse_args(); evidence = Evidence(args.evidence, args.executable)
    try:
        evidence.bind_executable('example', args.executable)
        if args.native_test: evidence.bind_executable('native_test', args.native_test)
        with tempfile.TemporaryDirectory(prefix='ow-gltf-') as temporary:
            output = Path(temporary) / 'example'
            tail = ['--example', 'meshes.import_gltf', '--headless', '--seed', '7', '--verify', '--output']
            result = evidence.run([str(args.executable), *tail, str(output)], ['<examples>', *tail, '<output>'])
            require(result.returncode == 0, 'native GLB fixture executes')
            actual = strict_json((output / 'result.json').read_bytes()); validate(actual)
            raw = (output / 'seed7.glb').read_bytes(); exact(raw, glb(), 'actual generated GLB file')
            exact(digest(raw), actual['file_sha256'], 'actual GLB file SHA256')
            evidence.retain_json('native/actual.json', actual); evidence.retain_json('native/expected.json', expected())
        if args.native_test:
            result = evidence.run([str(args.native_test)], ['<gltf_native_test>'])
            require(result.returncode == 0, 'native GLB boundary suite executes')
            summary = strict_json(result.stdout.encode('utf-8'))
            require(type(summary) is dict and set(summary) == {'status', 'assertions'} and summary['status'] == 'passed'
                    and type(summary['assertions']) is int and summary['assertions'] == 4090, 'native GLB assertion summary')
            evidence.retain_json('native/assertions.json', summary)
        evidence.finish('passed'); print('gltf oracle passed: ' + str(len(evidence.manifest['assertions'])) + ' assertions'); return 0
    except Failure as error: evidence.finish('failed', str(error)); raise
    except Exception: evidence.finish('failed', 'unexpected private internal error'); raise


if __name__ == '__main__': raise SystemExit(main_guard(main))
