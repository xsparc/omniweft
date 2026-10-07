#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Independent literal asset-retention reasons, binary bytes and collection effects."""
import argparse
import copy
import struct
import tempfile
from pathlib import Path

from hierarchy_oracle import Evidence as PriorEvidence
from sdk_test_support import ROOT, CHECKS, Failure, digest, exact, require, strict_json, main_guard


def sized(value):
    raw = value.encode('ascii')
    return struct.pack('<H', len(raw)) + raw


def manifest_bytes(value):
    return (b'OWAMNF01' + struct.pack('<I', value['format_version']) + bytes.fromhex(value['content_hash'])
            + struct.pack('<I', value['decoded_length']) + sized(value['media_type'])
            + sized(value['source']) + sized(value['license']))


def fixture():
    x, y = bytes([7, 7, 7, 14, 21, 21, 28, 35]), bytes([0, 7, 14, 21])
    def manifest(raw, source):
        return {'format_version': 1, 'content_hash': digest(raw), 'decoded_length': len(raw),
                'media_type': 'application/octet-stream', 'source': source, 'license': 'Apache-2.0'}
    manifests = [manifest(x, 'generated:seed7/a'), manifest(x, 'generated:seed7/a-alt'),
                 manifest(y, 'generated:seed7/c')]
    assets = [{'id': digest(manifest_bytes(m)), 'manifest': m} for m in manifests]
    blobs = [{'hash': digest(raw), 'bytes_hex': raw.hex()} for raw in (x, y)]
    return assets, blobs


def bundle(assets, blobs):
    out = b'OWASB001' + struct.pack('<III', 1, len(assets), len(blobs))
    for asset in assets:
        encoded = manifest_bytes(asset['manifest'])
        out += bytes.fromhex(asset['id']) + struct.pack('<I', len(encoded)) + encoded
    for blob in blobs:
        raw = bytes.fromhex(blob['bytes_hex'])
        out += sized('blobs/' + blob['hash']) + struct.pack('<BII', 0, len(raw), len(raw)) + raw
    return out


def snapshot(revision, assets=(), blobs=(), current=(), history=()):
    return {'format_version': 1, 'revision': revision,
            'assets': sorted(copy.deepcopy(assets), key=lambda a: a['id']),
            'blobs': sorted(copy.deepcopy(blobs), key=lambda b: b['hash']),
            'current_roots': sorted(current),
            'history_roots': [{'name': name, 'asset_ids': sorted(ids)} for name, ids in sorted(history)]}


def exported(state):
    return bundle(state['assets'], state['blobs']).hex()


def receipt(revision, imported=(), removed=0, blobs_removed=0):
    return {'status': 'committed', 'revision': revision, 'code': '', 'imported': sorted(imported),
            'manifests_removed': removed, 'blobs_removed': blobs_removed}


def command(label, request, state, imported=()):
    return {'case': label, 'command': request, 'expected_revision': state['revision'] - 1,
            'receipt': receipt(state['revision'], imported), 'snapshot': copy.deepcopy(state)}


def explanation(state, removable_assets, removable_blobs):
    # Expected rows come only from literal fixture relationships, never SDK/native code.
    assets = []
    for asset in state['assets']:
        assets.append({'id': asset['id'], 'content_hash': asset['manifest']['content_hash'],
                       'current_root': asset['id'] in state['current_roots'],
                       'history_roots': [h['name'] for h in state['history_roots'] if asset['id'] in h['asset_ids']]})
    blobs = []
    for blob in state['blobs']:
        referencing = [a for a in assets if a['content_hash'] == blob['hash']]
        blobs.append({'hash': blob['hash'], 'referencing_assets': [a['id'] for a in referencing],
                      'retaining_assets': [a['id'] for a in referencing if a['current_root'] or a['history_roots']]})
    return {'status': 'ok', 'code': '', 'revision': state['revision'],
            'report': {'format_version': 1, 'current_roots': copy.deepcopy(state['current_roots']),
                       'history_roots': copy.deepcopy(state['history_roots']), 'assets': assets, 'blobs': blobs,
                       'removable_assets': sorted(removable_assets), 'removable_blobs': sorted(removable_blobs)}}


def collection(state, removable_assets, removable_blobs):
    # A fresh owner acquires its OWN revision through Import and each explicit root command.
    ids = [a['id'] for a in state['assets']]
    fresh = snapshot(1, state['assets'], state['blobs'])
    steps = [command('import', {'type': 'import', 'bundle_hex': exported(state)}, fresh, ids)]
    fresh['revision'] = 2; fresh['current_roots'] = copy.deepcopy(state['current_roots'])
    steps.append(command('current', {'type': 'set_current_roots', 'asset_ids': state['current_roots']}, fresh))
    for history in state['history_roots']:
        fresh['revision'] += 1; fresh['history_roots'].append(copy.deepcopy(history))
        steps.append(command('history', {'type': 'set_history_roots', **history}, fresh))
    after = copy.deepcopy(fresh); after['revision'] += 1
    after['assets'] = [a for a in after['assets'] if a['id'] not in removable_assets]
    after['blobs'] = [b for b in after['blobs'] if b['hash'] not in removable_blobs]
    return {'steps': steps, 'before': fresh, 'export_before_hex': exported(fresh),
            'receipt': receipt(after['revision'], removed=len(removable_assets), blobs_removed=len(removable_blobs)),
            'after': after, 'export_after_hex': exported(after),
            'removed_assets': sorted(removable_assets), 'removed_blobs': sorted(removable_blobs)}


def query(label, state, removable_assets=(), removable_blobs=(), stale=None):
    result = (explanation(state, removable_assets, removable_blobs) if stale is None else
              {'status': 'rejected', 'code': 'REVISION_CONFLICT', 'revision': state['revision'], 'report': None})
    return {'case': label, 'expected_revision': state['revision'] if stale is None else stale,
            'before': copy.deepcopy(state), 'after': copy.deepcopy(state),
            'export_before_hex': exported(state), 'export_after_hex': exported(state), 'result': result,
            'collection': collection(state, removable_assets, removable_blobs) if stale is None else None}


def expected():
    assets, blobs = fixture(); aid, bid, cid = [a['id'] for a in assets]; xid, yid = [b['hash'] for b in blobs]
    input_hex = bundle(list(reversed(assets)), list(reversed(blobs))).hex()
    states = {0: snapshot(0)}
    states[1] = snapshot(1, assets, blobs)
    states[2] = snapshot(2, assets, blobs, [aid])
    states[3] = snapshot(3, assets, blobs, [aid], [('alpha', [aid])])
    states[4] = snapshot(4, assets, blobs, [aid], [('alpha', [aid]), ('beta', [bid])])
    states[5] = snapshot(5, assets, blobs, [aid], [('alpha', [aid]), ('beta', [bid]), ('empty', [])])
    states[6] = snapshot(6, assets, blobs, [], [('alpha', [aid]), ('beta', [bid]), ('empty', [])])
    states[7] = snapshot(7, assets, blobs, [], [('beta', [bid]), ('empty', [])])
    states[8] = snapshot(8, assets, blobs, [], [('empty', [])])
    states[9] = snapshot(9, assets, blobs, [aid], [('empty', [])])
    commands = [command('import', {'type': 'import', 'bundle_hex': input_hex}, states[1], [aid, bid, cid])]
    requests = [('current-a', {'type': 'set_current_roots', 'asset_ids': [aid]}),
                ('alpha-a', {'type': 'set_history_roots', 'name': 'alpha', 'asset_ids': [aid]}),
                ('beta-b', {'type': 'set_history_roots', 'name': 'beta', 'asset_ids': [bid]}),
                ('empty-history', {'type': 'set_history_roots', 'name': 'empty', 'asset_ids': []}),
                ('drop-current', {'type': 'set_current_roots', 'asset_ids': []}),
                ('drop-alpha', {'type': 'remove_history_roots', 'name': 'alpha'}),
                ('drop-beta', {'type': 'remove_history_roots', 'name': 'beta'}),
                ('restore-current', {'type': 'set_current_roots', 'asset_ids': [aid]})]
    commands += [command(label, request, states[revision]) for revision, (label, request) in enumerate(requests, 2)]
    # Freeze each removal set explicitly; shared content remains through B after A loses its roots.
    queries = [query('empty', states[0]), query('all-roots', states[5], [cid], [yid]),
               query('history-only', states[6], [cid], [yid]), query('stale', states[7], stale=6),
               query('shared-blob', states[7], [aid, cid], [yid]),
               query('unrooted', states[8], [aid, bid, cid], [xid, yid]),
               query('restored', states[9], [bid, cid], [yid])]
    original = explanation(states[5], [cid], [yid]); mutated = copy.deepcopy(original)
    mutated.update(status='rejected', code='DETACHED_EDIT', revision=0)
    r = mutated['report']; zero = '0' * 64
    r['format_version'] = 0; r['current_roots'] = [zero]
    r['history_roots'][0] = {'name': 'edited', 'asset_ids': [zero]}
    first = r['assets'][0]
    first.update(id=zero, content_hash=zero, current_root=not first['current_root'], history_roots=['edited'])
    r['blobs'][0] = {'hash': zero, 'referencing_assets': [], 'retaining_assets': [zero]}
    r['removable_assets'] = [zero]; r['removable_blobs'] = [zero]
    detached = {'original': original, 'mutated': mutated, 'fresh': copy.deepcopy(original),
                'before': copy.deepcopy(states[5]), 'after': copy.deepcopy(states[5]),
                'export_before_hex': exported(states[5]), 'export_after_hex': exported(states[5])}
    return {'schema_version': 1, 'example': 'assets.explain_retention', 'seed': 7, 'verified': True,
            'fixture': {'input_bundle_hex': input_hex, 'A': aid, 'B': bid, 'C': cid, 'X': xid, 'Y': yid},
            'commands': commands, 'queries': queries, 'detached': detached}


def validate(value):
    exact(value, expected(), 'complete independent retention proof')
    for item in value['queries']:
        exact(item['before'], item['after'], 'query preserves every owner field and revision')
        exact(item['export_before_hex'], item['export_after_hex'], 'query preserves all canonical export bytes')
        if item['collection'] is not None:
            collected = item['collection']; before, after = collected['before'], collected['after']
            for field, identity, removed_field in (('assets', 'id', 'removed_assets'), ('blobs', 'hash', 'removed_blobs')):
                difference = sorted({x[identity] for x in before[field]} - {x[identity] for x in after[field]})
                exact(difference, collected[removed_field], 'actual collected identities match frozen impact')
                exact(difference, item['result']['report'][removed_field.replace('removed_', 'removable_')],
                      'inspection predicts actual collection in reconstructed owner')
            exact(exported(before), collected['export_before_hex'], 'independent reconstructed export before collection')
            exact(exported(after), collected['export_after_hex'], 'independent reconstructed export after collection')
    require(value['detached']['original'] != value['detached']['mutated'], 'detached result was actually modified')
    exact(value['detached']['original'], value['detached']['fresh'], 'modified report does not alter fresh owner query')


class Evidence(PriorEvidence):
    def __init__(self, directory, executable):
        super().__init__(directory, executable)
        for name in ('tests/retention_oracle.py', 'tests/retention_oracle_test.py', 'tests/retention_native_test.cpp',
                     'tests/hierarchy_oracle.py', 'tests/sdk_test_support.py', 'tests/render_test_support.py',
                     '.github/workflows/native-quality.yml', 'cmake/ToolchainPolicy.cmake',
                     'tools/bootstrap.py', 'toolchains/bootstrap.json'):
            self.sources[name] = digest((ROOT / name).read_bytes())
        self.manifest.update(work_item='FP-008', example='assets.explain_retention', source_sha256=self.sources,
            limitations=['Read-only diagnostic on the existing volatile, native owner-only Catalog.',
                         'History roots are explicit owner-held sets, not World undo references.',
                         'Collection verification reconstructs a separate Catalog through ordinary typed commands.',
                         'No SDK, World, durable-store, dependency-graph, GPU or physics support is claimed.'])


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--executable', required=True, type=Path)
    parser.add_argument('--native-test', type=Path); parser.add_argument('--evidence', type=Path)
    args = parser.parse_args(); evidence = Evidence(args.evidence, args.executable)
    try:
        evidence.bind_executable('example', args.executable)
        if args.native_test: evidence.bind_executable('native_test', args.native_test)
        with tempfile.TemporaryDirectory(prefix='ow-retention-') as temporary:
            base = Path(temporary); output = base / 'example'
            tail = ['--example', 'assets.explain_retention', '--headless', '--seed', '7', '--verify', '--output']
            command_line = [str(args.executable), *tail, str(output)]
            public = ['<examples>', *tail, '<fresh-output>']
            result = evidence.run(command_line, public)
            require(result.returncode == 0, 'native retention example executes')
            path = output / 'result.json'
            require(path.is_file() and not path.is_symlink() and path.stat().st_size <= 262144,
                    'retention report is a bounded ordinary file')
            raw = path.read_bytes(); actual = strict_json(raw); validate(actual)
            evidence.retain_json('native/actual.json', actual); evidence.retain_json('native/expected.json', expected())
            result = evidence.run(command_line, ['<examples>', *tail, '<occupied-output>'])
            occupied_exit = result.returncode
            require(occupied_exit == 2 and path.read_bytes() == raw, 'occupied output rejects without overwriting evidence')
            bad_tail = ['--example', 'assets.explain_retention', '--headless', '--seed', '8', '--verify', '--output']
            result = evidence.run([str(args.executable), *bad_tail, str(base / 'bad')], ['<examples>', *bad_tail, '<fresh-output>'])
            seed_exit = result.returncode
            require(seed_exit == 2 and not (base / 'bad').exists(), 'unsupported seed rejects before output publication')
            recovered = base / 'recovered'
            result = evidence.run([str(args.executable), *tail, str(recovered)], ['<examples>', *tail, '<recovery-output>'])
            require(result.returncode == 0, 'fresh output recovers after CLI rejection')
            exact((recovered / 'result.json').read_bytes(), raw, 'repeated fixture has identical complete result bytes')
            evidence.retain_json('native/cli-recovery.json', {'occupied_output_exit': occupied_exit, 'unsupported_seed_exit': seed_exit,
                                                           'recovery_exit': result.returncode, 'result_sha256': digest(raw)})
        if args.native_test:
            result = evidence.run([str(args.native_test)], ['<retention_native_test>'])
            require(result.returncode == 0, 'native retention boundary suite executes')
            summary = strict_json(result.stdout.encode('utf-8'))
            require(type(summary) is dict and summary.keys() == {'status', 'assertions'} and summary['status'] == 'passed'
                    and type(summary['assertions']) is int and 553 <= summary['assertions'] <= 6589,
                    'native retention suite reports its actual bounded assertion count')
            evidence.retain_json('native/assertions.json', summary)
        evidence.finish('passed')
        print('retention oracle passed: ' + str(len(evidence.manifest['assertions'])) + ' assertions'); return 0
    except Failure as error: evidence.finish('failed', str(error)); raise
    except Exception: evidence.finish('failed', 'unexpected private internal error'); raise


if __name__ == '__main__': raise SystemExit(main_guard(main))
