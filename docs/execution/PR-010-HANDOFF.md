# PR-010 handoff

State: implementation verified locally; draft delivery in preparation. Branch `codex/pr-010-bounded-queries`; base `4996aca01f7bfa76cacc50eb75aa6ffe0f042b3a`. Root owns writes and the backlog; all older worktrees are preserved. Scope: [plan](PR-010-PLAN.md), [authorization](AUTHORIZATION.md), [runnable example](../../examples/observe-semantic_query.md).

## Implemented behavior

Native typed `entity.tags.set` stages bounded, sorted metadata atomically. `ow::observe::Session` binds an immutable host read grant to one World and returns permission-filtered, UUID-sorted tag/AABB results with bounded complete JSON pages. Opaque native cursors retain one bounded snapshot across mutations, support replay, and require resync after replacement or absolute expiry. Default grants deny all; tags confer no authority. The World must outlive its single-owner session.

Tagged diagnostic data uses OWOBJ003; untagged root/hierarchy bytes remain OWOBJ001/OWOBJ002. Existing remote/SDK/policy profiles explicitly deny native tag operations and tagged observations. No remote query endpoint, grant/quota change, dependency or save migration is added.

## Verification and independent review

Windows Release build passed with MSVC 19.44.35227, CMake 3.31.6, Ninja 1.13.2 and Python 3.12.14; Vulkan disabled. Full CTest ran all 26 checks: 25 passed, with the new oracle schema selftest failing a test-only expectation for two equivalent regex forms. That expectation was corrected, and the affected selftest passed. Independent review then identified terminal-newline acceptance in the new JSON Schema tag pattern; the pattern now uses an absolute-end assertion and its accepted/rejected boundary selftest passes. Production query code was unchanged by these refinements. Planning validation and whitespace checks passed.

Independent architecture, source and oracle review found no remaining blocking issue after the tag-schema correction. The review also prompted a longer real expiry interval in the example to reduce scheduling flakiness. Literal oracle geometry is checked at the preregistered 1e-10 tolerance; identity, revision, tags, framing and diagnostic bytes are exact. Native tests cover hidden-row noninterference, fixed deadlines, generation reuse, byte limits, invalid host grants, rollback and remote policy rejection. Required Linux hosted checks and clean-candidate retained archive audit are pending. This is a CPU-only slice; no GPU or physics evidence is claimed.

## Delivery and next action

PR-009 is integrated: its reviewed delivery tree matches maintainer merge 4996aca, and all six required post-merge checks passed. This PR reconciles those durable records.

Commit the reviewed runtime, retain clean-candidate evidence, independently audit the public archive, then publish the authorized draft and inspect actual latest-head hosted checks. Keep PR-011 proposed until actual maintainer review/certification and merge, matching merge-tree verification and all required post-merge checks. No auto-merge, automatic ready transition or history rewrite is authorized.
