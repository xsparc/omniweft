#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Corrupt semantic reasons, shared-blob effects, revisions and detached ownership proof."""
import copy
from retention_oracle import CHECKS, Failure, expected, validate


def main():
    original = expected(); validate(original); cases = []

    def change(label, edit):
        value = copy.deepcopy(original); edit(value); cases.append((label, value))

    def replace(path, value):
        def edit(report):
            node = report
            for key in path[:-1]: node = node[key]
            node[path[-1]] = value
        return edit

    for path, value in [
        (('schema_version',), True), (('seed',), 7.0), (('verified',), 1),
        (('fixture', 'A'), '0' * 64), (('fixture', 'input_bundle_hex'), '00'),
        (('queries', 1, 'result', 'report', 'format_version'), 1.0),
        (('queries', 1, 'result', 'revision'), 6),
        (('queries', 1, 'result', 'status'), 'committed'),
        (('queries', 1, 'result', 'code'), 'REVISION_CONFLICT'),
        (('queries', 1, 'result', 'report', 'assets', 0, 'current_root'), 1),
        (('queries', 1, 'result', 'report', 'assets', 0, 'history_roots'), ['invented']),
        (('queries', 1, 'result', 'report', 'assets', 0, 'content_hash'), '0' * 64),
        (('queries', 1, 'after', 'revision'), 6),
        (('queries', 1, 'export_after_hex'), '00'),
        (('queries', 3, 'result', 'report'), {}),
        (('queries', 3, 'result', 'code'), 'UNKNOWN_ASSET'),
        (('queries', 3, 'expected_revision'), 7),
        (('queries', 3, 'after', 'revision'), 8),
        (('queries', 4, 'collection', 'before', 'revision'), 7),
        (('queries', 4, 'collection', 'receipt', 'blobs_removed'), 2),
        (('queries', 4, 'collection', 'after', 'revision'), True),
        (('queries', 4, 'collection', 'export_after_hex'), '00'),
        (('queries', 4, 'collection', 'removed_assets'), []),
        (('queries', 4, 'collection', 'steps', 0, 'snapshot', 'current_roots'), ['0' * 64]),
        (('queries', 4, 'collection', 'steps', 0, 'receipt', 'revision'), 7),
        (('queries', 5, 'result', 'report', 'removable_blobs'), []),
        (('queries', 6, 'result', 'report', 'removable_assets'), []),
        (('commands', 8, 'receipt', 'revision'), 8),
        (('detached', 'after', 'revision'), 0),
        (('detached', 'export_after_hex'), '00'),
        (('detached', 'fresh', 'report', 'format_version'), 0),
    ]:
        change('field corruption ' + '/'.join(map(str, path)), replace(path, value))

    def corrupt_shared(report):
        row = next(b for b in report['queries'][4]['result']['report']['blobs'] if b['hash'] == report['fixture']['X'])
        row['retaining_assets'] = []
    change('shared blob loses its surviving B retainer', corrupt_shared)

    def hide_unrooted_reference(report):
        row = next(b for b in report['queries'][4]['result']['report']['blobs'] if b['hash'] == report['fixture']['X'])
        row['referencing_assets'] = [report['fixture']['B']]
    change('unrooted A incorrectly disappears from all referencing manifests', hide_unrooted_reference)

    def lose_alpha(report):
        row = next(a for a in report['queries'][2]['result']['report']['assets'] if a['id'] == report['fixture']['A'])
        row['history_roots'] = []
    change('clearing current A incorrectly clears alpha retention', lose_alpha)

    def collect_shared_blob(report):
        report['queries'][4]['result']['report']['removable_blobs'].append(report['fixture']['X'])
        report['queries'][4]['collection']['removed_blobs'].append(report['fixture']['X'])
        report['queries'][4]['collection']['after']['blobs'] = []
    change('matching but false inspector and collection reports cannot agree themselves through the oracle', collect_shared_blob)
    change('empty named history root omitted', lambda r: r['queries'][1]['result']['report']['history_roots'].pop())
    change('history order changed', lambda r: r['queries'][1]['result']['report']['history_roots'].reverse())
    change('duplicate retention reason', lambda r: r['queries'][1]['result']['report']['current_roots'].append(r['fixture']['A']))
    change('detached mutation was fabricated', lambda r: r['detached'].update(mutated=copy.deepcopy(r['detached']['original'])))
    change('detached mutation leaked into fresh query', lambda r: r['detached'].update(fresh=copy.deepcopy(r['detached']['mutated'])))
    change('unknown public field', lambda r: r.update(private_path='unexpected'))

    for label, value in cases:
        try: validate(value)
        except Failure: pass
        else: raise RuntimeError('corrupt retention proof accepted: ' + label)
    CHECKS.clear()
    print('retention oracle selftest passed: ' + str(len(cases)) + ' corruptions'); return 0


if __name__ == '__main__': raise SystemExit(main())
