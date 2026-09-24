# PR-012: authoring undo and redo

Authority: adopted PR-001 through PR-038 scope in [AUTHORIZATION.md](AUTHORIZATION.md), reaffirmed by the maintainer continuation after PR #17 squash merge. Dependencies PR-009 and PR-011 are integrated. Base `457286e85818381f56aab4b021da0fb5439acdec`; branch `codex/pr-012-authoring-undo`. Root owns writes; independent architecture/source/oracle/evidence review is required.

## Supported behavior

Provide a native, noncopyable, World-bound owner-thread History for existing generation-qualified tag edits and transforms on isolated root objects. Every forward, inverse and redo passes through Coordinator and any caller-supplied live StagingGuard. Exact authored content returns to previous values while world and affected entity revisions remain monotonic. Identity, generation, allocator, diagnostic bytes and transaction admission contracts remain intact. No mutable snapshot restoration is introduced.

History has eight entries, at most four operations per batch and 16,384-byte canonical forward/inverse envelope bounds. Typed strings and vectors are bounded before copying/serializing. Reserve record storage before forward publication; update storage/cursor only through noexcept moves after commit. Reject capacity exhaustion before mutation. A successful new edit after undo discards the redo tail; any failure preserves world and history. Header-only undo/redo requests retain caller-supplied correlation, fresh admission metadata, expected revision and budgets; history does not own authentication or exactly-once admission.

Both caller revision and private history-head revision must equal the live world. Outside authoring changes make old history explicitly stale; clear/reconciliation discards records without changing the world. Physical-time rewind requests are unsupported. Creation/deletion, reparenting, transforms on parents/parented objects, remote SDK history and persistence are deferred: current operations cannot guarantee exact inverse identity or hierarchy floating-point restoration. Tags may address hierarchy objects because they do not change transform/parent values. These are supported-edit limits within the adopted example, not a new ADR or scope expansion.

## Validation

Implement world.undo_chain with full current snapshots and independently encoded canonical bytes at each checkpoint. Round-trip content equivalence excludes only revision counters; independently expected revisions remain checked. Include stale inverse, physical-time rejection, recovery, duplicate-property batches, atomic late failure, branch truncation, failed-branch preservation, capacity, bounded native inputs, synthetic staging-hook rejection and hierarchy restrictions. Run targeted then full Windows CTests; use hosted Windows/Linux checks. No GPU/physics claim.

Retain a clean-candidate allowlisted evidence archive, independently review source/oracle/archive, publish an authorized draft, and record exact hosted runs in the handoff before closeout. Keep PR-013 proposed until actual maintainer merge and post-merge verification. No automatic ready transition, merge, certification, history rewrite or trust change.

The requested OpenSteward workflow is applied to bounded scope and traceability using Omniweft's backlog/handoffs. Its bundled static and dated strict checker report a missing OpenSteward registry here; that foreign-project gate is not claimed passed. Omniweft's planning validator and required native/renderer jobs remain authoritative; no parallel governance authority or gate bypass is introduced.
