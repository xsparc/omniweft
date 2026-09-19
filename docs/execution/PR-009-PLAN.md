# PR-009 plan: bounded native hierarchy

Owner: Codex coordinator. Branch codex/pr-009-hierarchy; base 27552713dc95a776a21c7f5425c4cf672bc169fa. Scope: [authorization](AUTHORIZATION.md), [objects.hierarchy](../../examples/objects-hierarchy.md), ADR-0002. PR-003 dependency is integrated; PR-008 was independently reviewed and actually merged with six required post-merge checks passing.

## Behavior and compatibility

Add typed native entity.reparent with target, parent (identity/temporary target or null), and mode preserve_world or preserve_local. Keep transform.set and Entity.transform world-space semantics. Store generation-bearing parent identity and authored local TRS; recompute cached descendant world transforms atomically in staged stable order. Mutations remain inside Coordinator and global expected authoring revisions.

Only positive exactly uniform scale may be used by an entity with children. Existing roots/leaves retain signed/nonuniform scales. This closed TRS subset avoids implicit shear/decomposition. Reject invalid parent scales, nonfinite composition/inversion and scale underflow to zero. Derived quaternions are normalized after multiplication; existing root transforms retain original accepted values/bytes. Numerical oracle tolerance is 1e-10 for derived fixture values (meters/unitless); identities, revisions and rollback bytes are exact.

Bound graph traversal by max_slots (1024); no recursive unbounded walks. Resolve cycles/identities against each staged prefix. Parent deletion rejects while any child remains; no implicit cascade. Detach preserve_world retains cached world TRS; preserve_local promotes the child's prior local TRS. Reparent/transform conservatively advances the entire affected subtree to the transaction's next revision, including unchanged derived descendants. Root-only diagnostic format1/OWOBJ001 is unchanged. States with live parent edges use format2/OWOBJ002 containing cached world, parent and local data; removing every edge clears child metadata and restores format1. These are diagnostic bytes, not a save format or migration.

## Scope boundary

This slice implements the public typed native API and standalone native example, as permitted by the PR-003 dependency and CPU acceptance. Existing HTTP gateways reject the new operation before owner scheduling or sequence consumption, including mixed batches. Policy typed admission rejects it before serialization/staging and rejects preexisting hierarchy snapshots. Existing advertised remote operations, SDK models, grants, quotas, permissions and wire outputs remain unchanged. Hierarchy over the remote SDK/policy is unsupported pending an explicit compatible observation/admission contract. No physics bodies exist yet, so physics parenting rules remain deferred; no physics capability is claimed.

## Verification and delivery

Runnable objects.hierarchy uses the typed coordinator and accepts bounded JSON input fixtures. Independently authored expectations cover both reparent/detach modes, multilevel motion, cycles, late-failure rollback, allocation recovery, stale parent/child generations, ambiguous deletion and unsupported/numerical scale cases. Existing root canonical goldens must remain passing. Native presentation tests bind derived world transforms to actual packet vertices. Raw authenticated transport tests prove denied hierarchy consumes no sequence/state and policy usage recovers.

Run pinned Windows/Linux CPU builds/CTest and planning validation. GPU is not a PR-009 lane; packet tests and unchanged existing GPU fixture geometry do not claim new physical-GPU evidence. Use hosted Linux checks; conservative Docker only if available/needed. Preserve source/artifact hashes and allowlisted evidence; no personal paths, credentials/epochs, raw streams or binary executables in public. Independent implementation and evidence review precede final delivery. Coordinator owns all writes/ledger. No new dependency or asset. Ordinary PR revert is rollback; maintainer review/certification and actual merge remain required.

## Diagnostic layout and observation boundary

OWOBJ002 retains the complete OWOBJ001 field order described in [PR-003](PR-003-PLAN.md), changes the eight-byte magic to OWOBJ002, and appends one parent-present byte immediately after each live entity's ten world-TRS doubles. If present, it appends the parent's 36 ASCII UUID bytes, generation uint64 LE, then ten local-TRS binary64 LE values. If absent, no parent/local payload follows. Format2 JSON adds parent and local_transform keys to each live entity; roots carry null for both. Format1 bytes and JSON have no added fields. Empty/deleted slots keep the existing encoding. All zero doubles encode as positive zero.

The shared remote serializer rejects any non-format1 or parent-bearing snapshot, including runtime observations supplied through public native callbacks. Dedicated test hosts construct hierarchy only through public native Coordinator calls and exercise these real production gateways; no mutable-world test escape exists. Their policy callback dispatcher serializes execution under a deadline-aware timed mutex.
