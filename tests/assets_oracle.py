#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Independent seed-7 asset identities, binary encoder/decoder and complete state oracle."""
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


def canonical(m):
    return (b'OWAMNF01' + struct.pack('<I', m['format_version']) + bytes.fromhex(m['content_hash'])
            + struct.pack('<I', m['decoded_length']) + sized(m['media_type']) + sized(m['source']) + sized(m['license']))


def asset(m):
    return {'id': digest(canonical(m)), 'manifest': copy.deepcopy(m)}


def packed(data, codec=0, encoded=None, decoded=None, path=None):
    return (path or 'blobs/' + digest(data), codec, len(data) if decoded is None else decoded,
            data if encoded is None else encoded)


def bundle(manifests, blobs):
    out = b'OWASB001' + struct.pack('<III', 1, len(manifests), len(blobs))
    for m in manifests:
        raw = canonical(m)
        out += bytes.fromhex(digest(raw)) + struct.pack('<I', len(raw)) + raw
    for path, codec, decoded, encoded in blobs:
        out += sized(path) + struct.pack('<BII', codec, decoded, len(encoded)) + encoded
    return out


def fixtures():
    data = bytes([7, 7, 7, 14, 21, 21, 28, 35])
    other = bytes([0, 7, 14, 21])
    def m(raw, source):
        return {'format_version': 1, 'content_hash': digest(raw), 'decoded_length': len(raw),
                'media_type': 'application/octet-stream', 'source': source, 'license': 'Apache-2.0'}
    return data, other, m(data, 'generated:seed7/a'), m(data, 'generated:seed7/a-alt'), m(other, 'generated:seed7/c')


def snapshot(revision, manifests=(), blobs=(), current=(), history=()):
    return {'format_version': 1, 'revision': revision,
            'assets': sorted([asset(m) for m in manifests], key=lambda a: a['id']),
            'blobs': sorted([{'hash': digest(b), 'bytes_hex': b.hex()} for b in blobs], key=lambda b: b['hash']),
            'current_roots': sorted(current),
            'history_roots': [{'name': name, 'asset_ids': sorted(ids)} for name, ids in sorted(history)]}


def receipt(revision, imported=(), removed=0, blobs_removed=0, code=''):
    return {'status': 'rejected' if code else 'committed', 'revision': revision, 'code': code,
            'imported': sorted(imported), 'manifests_removed': removed, 'blobs_removed': blobs_removed}


def expected():
    data, other, a, b, c = fixtures()
    aid, bid, cid = [asset(m)['id'] for m in (a, b, c)]
    history = [('undo-1', [aid])]
    steps = []
    def step(label, s, imported=(), removed=0, blobs_removed=0):
        steps.append({'case': label, 'receipt': receipt(s['revision'], imported, removed, blobs_removed), 'snapshot': s})
    step('import-a', snapshot(1, [a], [data]), [aid])
    step('deduplicate-a', snapshot(2, [a], [data]), [aid])
    step('provenance-and-content', snapshot(3, [a, b, c], [data, other]), [bid, cid])
    step('current-roots', snapshot(4, [a, b, c], [data, other], [bid, cid]))
    step('history-root', snapshot(5, [a, b, c], [data, other], [bid, cid], history))
    step('drop-current-c', snapshot(6, [a, b, c], [data, other], [bid], history))
    step('history-retains-a', snapshot(7, [a, b], [data], [bid], history), removed=1, blobs_removed=1)
    step('remove-history', snapshot(8, [a, b], [data], [bid]))
    step('shared-blob-survives', snapshot(9, [b], [data], [bid]), removed=1)
    step('drop-last-root', snapshot(10, [b], [data]))
    step('reclaim-last-blob', snapshot(11), removed=1, blobs_removed=1)
    step('reimport-recovery', snapshot(12, [a], [data]), [aid])
    step('restore-current', snapshot(13, [a], [data], [aid]))
    step('post-rejection-recovery', snapshot(14, [a], [data], [aid], [('undo-2', [aid])]))
    exported = bundle(sorted([a, b, c], key=lambda m: asset(m)['id']),
                      [packed(v) for v in sorted([data, other], key=digest)])
    a_bundle = bundle([a], [packed(data)])
    rle = bundle([a], [packed(data, 1, bytes([3, 7, 1, 14, 2, 21, 1, 28, 1, 35]))])
    bad_id = bytearray(a_bundle); bad_id[20] ^= 1
    bad_version = bytearray(a_bundle); bad_version[8] = 2
    corrupt = bytes([6, 7, 7, 14, 21, 21, 28, 35])
    negatives = [
        ('missing-blob', bundle([a], []), 'MISSING_BLOB'),
        ('traversal-path', bundle([a], [packed(data, path='../blobs/' + digest(data))]), 'INVALID_PATH'),
        ('wrong-content-hash', bundle([a], [packed(data, encoded=corrupt)]), 'HASH_MISMATCH'),
        ('wrong-manifest-hash', bad_id, 'HASH_MISMATCH'),
        ('decompression-overflow', bundle([a], [packed(data, 1, bytes([9, 7]))]), 'DECOMPRESSION_ERROR'),
        ('decompression-zero', bundle([a], [packed(data, 1, bytes([0, 7]))]), 'DECOMPRESSION_ERROR'),
        ('decompression-truncated', bundle([a], [packed(data, 1, bytes([8]))]), 'DECOMPRESSION_ERROR'),
        ('unknown-version', bad_version, 'UNSUPPORTED_VERSION'),
        ('unknown-codec', bundle([a], [packed(data, 9)]), 'INVALID_BUNDLE'),
        ('trailing-data', a_bundle + b'\0', 'INVALID_BUNDLE'),
        ('duplicate-manifest', bundle([a, a], [packed(data)]), 'DUPLICATE_ENTRY'),
        ('duplicate-blob', bundle([a], [packed(data), packed(data)]), 'DUPLICATE_ENTRY'),
        ('extra-blob', bundle([a], [packed(data), packed(other)]), 'EXTRA_BLOB'),
        ('decoded-budget', bundle([a], [packed(data, decoded=65537)]), 'BUDGET_EXCEEDED'),
        ('stale-revision', None, 'REVISION_CONFLICT'), ('unknown-root', None, 'UNKNOWN_ASSET')]
    unchanged = snapshot(13, [a], [data], [aid])
    return {'schema_version': 1, 'example': 'assets.roundtrip', 'seed': 7, 'verified': True,
            'detached_query_unchanged': True, 'steps': steps,
            'roundtrip': {'bundle_hex': exported.hex(), 'file_sha256': digest(exported),
                          'receipt': receipt(1, [aid, bid, cid]), 'snapshot': snapshot(1, [a, b, c], [data, other]),
                          'export_hex': exported.hex()},
            'rle': {'input_hex': rle.hex(), 'receipt': receipt(1, [aid]), 'snapshot': snapshot(1, [a], [data]),
                    'export_hex': a_bundle.hex()},
            'negative': [{'case': label, 'input_hex': raw.hex() if raw is not None else None,
                          'input_unchanged': True, 'receipt': receipt(13, code=code),
                          'before': copy.deepcopy(unchanged), 'after': copy.deepcopy(unchanged)} for label, raw, code in negatives]}


def decode(raw):
    """Independent strict decoder used on actual export and RLE bytes, never runtime declarations."""
    require(len(raw) <= 524288, 'oracle bundle byte budget')
    cursor = 0
    def take(n):
        nonlocal cursor
        require(0 <= n <= len(raw) - cursor, 'oracle binary span')
        value = raw[cursor:cursor+n]; cursor += n
        return value
    def integer(n):
        return int.from_bytes(take(n), 'little')
    def string(limit):
        n = integer(2); require(1 <= n <= limit, 'oracle bounded string')
        value = take(n); require(all(32 <= b <= 126 for b in value), 'oracle printable ASCII')
        return value.decode('ascii')
    exact(take(8), b'OWASB001', 'oracle bundle magic')
    exact(integer(4), 1, 'oracle bundle schema')
    count, blob_count = integer(4), integer(4)
    require(count <= 8 and blob_count <= 8, 'oracle entry caps')
    assets, blobs = [], []
    for _ in range(count):
        identity = take(32).hex(); n = integer(4)
        require(57 <= n <= 310, 'oracle manifest length bound')
        end = cursor + n; start = cursor
        exact(take(8), b'OWAMNF01', 'oracle manifest magic')
        version, sha, length = integer(4), take(32).hex(), integer(4)
        exact(version, 1, 'oracle manifest schema'); require(length <= 65536, 'oracle decoded bound')
        m = {'format_version': version, 'content_hash': sha, 'decoded_length': length,
             'media_type': string(64), 'source': string(128), 'license': string(64)}
        exact(cursor, end, 'oracle manifest consumes declared bytes')
        exact(digest(raw[start:end]), identity, 'oracle manifest SHA256')
        require(not any(v['id'] == identity for v in assets), 'oracle unique asset identity')
        assets.append({'id': identity, 'manifest': m})
    total = 0
    for _ in range(blob_count):
        path = string(70)
        require(len(path) == 70 and path.startswith('blobs/') and all(c in '0123456789abcdef' for c in path[6:]),
                'oracle exact blob identifier')
        codec, length, encoded_length = integer(1), integer(4), integer(4)
        total += length
        require(length <= 65536 and total <= 262144, 'oracle decompression budget')
        encoded = take(encoded_length)
        if codec == 0:
            decoded = encoded
        else:
            exact(codec, 1, 'oracle supported codec')
            require(len(encoded) % 2 == 0, 'oracle complete RLE pairs')
            decoded = bytearray()
            for i in range(0, len(encoded), 2):
                run = encoded[i]
                require(0 < run <= length - len(decoded), 'oracle RLE expansion stays within declared output')
                decoded.extend([encoded[i+1]] * run)
        exact(len(decoded), length, 'oracle decoded size'); exact(digest(decoded), path[6:], 'oracle content SHA256')
        require(not any(b['hash'] == path[6:] for b in blobs), 'oracle unique blob')
        blobs.append({'hash': path[6:], 'bytes_hex': decoded.hex()})
    exact(cursor, len(raw), 'oracle bundle has no trailing data')
    exact({a['manifest']['content_hash'] for a in assets}, {b['hash'] for b in blobs}, 'oracle exact referenced blob set')
    for a in assets:
        blob = next(b for b in blobs if b['hash'] == a['manifest']['content_hash'])
        exact(a['manifest']['decoded_length'], len(bytes.fromhex(blob['bytes_hex'])), 'oracle manifest content size')
    return sorted(assets, key=lambda a: a['id']), sorted(blobs, key=lambda b: b['hash'])


def validate(value):
    exact(value, expected(), 'complete literal asset retention proof')
    for label, field in [('roundtrip', 'bundle_hex'), ('roundtrip', 'export_hex'), ('rle', 'input_hex'), ('rle', 'export_hex')]:
        assets, blobs = decode(bytes.fromhex(value[label][field]))
        exact(assets, value[label]['snapshot']['assets'], 'independently decoded manifests')
        exact(blobs, value[label]['snapshot']['blobs'], 'independently decoded content')
    for step in value['steps']:
        for a in step['snapshot']['assets']:
            exact(digest(canonical(a['manifest'])), a['id'], 'manifest identity includes provenance')
    require(value['steps'][0]['snapshot']['assets'][0]['id'] != value['steps'][8]['snapshot']['assets'][0]['id'],
            'shared content does not collapse provenance identity')


class Evidence(PriorEvidence):
    def __init__(self, directory, executable):
        super().__init__(directory, executable)
        for name in ('tests/assets_oracle.py', 'tests/assets_oracle_test.py', 'tests/assets_native_test.cpp',
                     '.github/workflows/native-quality.yml', 'cmake/ToolchainPolicy.cmake', 'tools/bootstrap.py'):
            self.sources[name] = digest((ROOT / name).read_bytes())
        self.manifest.update(work_item='PR-016', example='assets.roundtrip', source_sha256=self.sources,
            limitations=['Volatile native owner-only asset Catalog and bounded opaque bundle; no durable asset store.',
                         'History roots are explicit owner retention sets, not World undo/persistence integration.',
                         'Provenance is retained attribution data; imported source/license assertions are not authenticated.',
                         'No glTF, remote asset endpoint, World attachment, GPU or physics evidence.'])


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--executable', required=True, type=Path)
    parser.add_argument('--native-test', type=Path); parser.add_argument('--evidence', type=Path)
    args = parser.parse_args(); evidence = Evidence(args.evidence, args.executable)
    try:
        evidence.bind_executable('example', args.executable)
        if args.native_test: evidence.bind_executable('native_test', args.native_test)
        with tempfile.TemporaryDirectory(prefix='ow-assets-') as temporary:
            output = Path(temporary) / 'example'
            tail = ['--example', 'assets.roundtrip', '--headless', '--seed', '7', '--verify', '--output']
            result = evidence.run([str(args.executable), *tail, str(output)], ['<examples>', *tail, '<output>'])
            require(result.returncode == 0, 'native asset fixture executes')
            actual = strict_json((output / 'result.json').read_bytes()); validate(actual)
            file = (output / 'seed7.owas').read_bytes()
            exact(file.hex(), actual['roundtrip']['bundle_hex'], 'actual bundle file bytes')
            exact(digest(file), actual['roundtrip']['file_sha256'], 'actual bundle file SHA256')
            evidence.retain_json('native/actual.json', actual); evidence.retain_json('native/expected.json', expected())
        if args.native_test:
            result = evidence.run([str(args.native_test)], ['<assets_native_test>'])
            require(result.returncode == 0, 'native asset boundary suite executes')
            summary = strict_json(result.stdout.encode('utf-8'))
            require(type(summary) is dict and set(summary) == {'status', 'assertions'} and summary['status'] == 'passed'
                    and type(summary['assertions']) is int and summary['assertions'] == 853, 'native asset assertion summary')
            evidence.retain_json('native/assertions.json', summary)
        evidence.finish('passed'); print('assets oracle passed: ' + str(len(evidence.manifest['assertions'])) + ' assertions'); return 0
    except Failure as error: evidence.finish('failed', str(error)); raise
    except Exception: evidence.finish('failed', 'unexpected private internal error'); raise


if __name__ == '__main__': raise SystemExit(main_guard(main))
