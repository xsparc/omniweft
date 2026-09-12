# PR-002 execution handoff

- State: **in_review**; [GitHub PR #5](https://github.com/xsparc/omniweft/pull/5), pending maintainer integration. Do not mark done before an actual merge.
- Work item: PR-002 — Versioned command schema; example `protocol.reject_invalid`.
- Task: `01a09699-3e74-7103-80e4-63d80a8e978e`; coordinator branch `codex/pr-002-command-schema`.
- Base: PR-001 squash merge `7283b8ad02b96195bb9e828b9945dc4c6c975c9a`, verified through GitHub after the user's merge confirmation. PR-001 is done in the shared ledger.
- Authority: [adopted implementation scope](AUTHORIZATION.md). Future merge and contribution-certification policy is unchanged.
- Contract: [slice plan](PR-002-PLAN.md), [schema 0.1](../../schemas/protocol-0.1.schema.json) and [public example](../../examples/protocol-reject_invalid.md).
- Native helper: `a4cfb4b5a451f9937d5ef4cf791adc650e682ec9` on `codex/pr-002-code`.
- Independent test helper: `5ceb16c99e655bc56ab53b3fa8d83e79187c6c03` on `codex/pr-002-tests`.
- Ownership: helper branches have separate file ownership; coordinator serializes shared schema, ledger, docs and workflow integration through Git trees and non-forced branch updates.
- Local execution: **not_run** for PR-002. Local command calls did not return, including a read-only diagnostic. GitHub API preparation and hosted Windows/Linux checks supplied the actual evidence; existing local worktrees remain preserved.

## Delivered behavior

The pure `ow_commands` library parses bounded JSON into typed envelopes and serializes validated typed values. It implements schema 0.1 with `entity.create` and `transform.set`, structured error code/path, exact unsigned 64-bit round-trips and stable serialized bytes. It rejects malformed UTF-8/JSON, raw NUL suffixes, decoded duplicate keys, unknown fields/versions/operations, invalid integer syntax, non-finite values and byte/depth/operation-budget violations.

The example exercises valid → invalid → valid recovery in one process. Independent fixtures also verify the public command without `--input`, preservation of existing output and missing-input failure. At most 64 input files are read, each capped at the envelope byte limit plus one byte to detect overflow.

The module receives no world, executor, authentication or capability context. A `schema_valid` result does not grant access, admit/schedule/commit a transaction or establish entity existence, live quotas, temporary-reference resolution or transform execution invariants. Rendering, physics, SDK and world mutation remain later slices.

## Observed hosted evidence

The final functional code/test/workflow head is `873cfaff0243acb9409a5eda7c16d05ea58b1a46`. GitHub tested merge candidate `6aa5137e1d229a2c87f03e4305d57ee1d8eb79b8`, whose parents are that head and base `7283b8ad02b96195bb9e828b9945dc4c6c975c9a`. Its tree equals the functional head tree `e4c1e588ddb7fe8495955ac7790f2a480d3f4116`. Both manifests report a clean worktree.

- [Native run 34724298685](https://github.com/xsparc/omniweft/actions/runs/34724298685): **passed** on Ubuntu 24.04 and Windows Server 2022; configure/build and CTest **4/4** per platform.
- [Planning run 34724298524](https://github.com/xsparc/omniweft/actions/runs/34724298524): **passed** document/ledger validation and all **13** planning regression tests on both platforms.
- Independent protocol oracle: **150** byte fixtures, **17** stable serialization round-trips, **3** built-in public-example outcomes; final retained manifest **1,609 assertions / 11 commands** per platform. The printed pre-exit count is 1,608; the final evidence-retention assertion brings the manifest count to 1,609.
- Typed native fixture: **30** NaN/positive-infinity/negative-infinity component cases with structured rejection and valid recovery.
- Real-validator mutation proof: **passed**, **16 assertions / 6 commands** per platform. A temporary source copy deliberately accepts unsupported versions; the independent oracle exits **1** on the expected `schema_invalid` versus `schema_valid` mismatch. Original source bytes remain unchanged.
- Bootstrap regression: **passed**, **75 assertions / 24 commands**, preserving the PR-001 result hash.

| Observed tool | Ubuntu 24.04 lane | Windows Server 2022 lane |
| --- | --- | --- |
| Compiler | Clang 18.1.3 | MSVC 19.44.35228.0, toolset 14.44.35207 |
| CMake | 3.31.6 | 3.31.6 |
| Ninja executable | 1.13.2.git.kitware.jobserver-pipe-1 | 1.13.2.git.kitware.jobserver-pipe-1 |
| Python | 3.12.10 | 3.12.10 |

The first protocol batch result SHA-256 is `4e5584fe5612d42d43b4a2bdaa1051f225fb2b69e6ea157c4976c2739b4f9024` on both platforms. Other batches, the built-in result, raw fixtures, all command records, mutation source/diff and CTest output are retained separately in the same artifacts. Bootstrap result SHA-256 remains `d04cf4bae0f94b089ce2d135f9f40ec56b31f30095dc798df46160b5acf0d6f8`.

Functional-run artifact IDs: Linux `10307735522`, ZIP SHA-256 `854e487741d4f9c50901e546ca5d62b2e6f16eee3becf0e5e724f27086a90595`; Windows `10307301988`, ZIP SHA-256 `637258b235db9b9092ead6b2fb5aa343a084b02194a030a9ede6d8951dd78e50`. Artifacts have seven-day retention; compact manifest summaries remain in the job logs. Fixtures and provenance remain versioned in source.

This handoff/ledger update changes documentation only. The PR must still pass checks on its current final head; exact final candidate/run links and review state are recorded in the PR and Actions rather than self-referencing this commit. Verify live checks before integration.

## Independent review and resolved findings

- Contract review found no actionable issues at `4a90580e384291b1097006607277abd1763b4f75`.
- Native source review found the JSON dependency interprets raw NUL as end-of-input. The fix rejects raw NUL after successful SAX and before DOM construction; independent trailing-NUL and NUL-plus-garbage fixtures verify rejection and subsequent recovery. Reviewer accepted native source `a4cfb4b5a451f9937d5ef4cf791adc650e682ec9` with no remaining findings.
- The planning false-completion regression initially inherited PR-001's legitimate new merge evidence. Its scratch fixture now explicitly removes execution evidence. Production validation and both rejection assertions remain unchanged; independent review accepted the correction and hosted regression checks pass.
- The first mutation-proof run detected the deliberately broken validator but expected a status diagnostic after an earlier field-shape check. The independent oracle now checks status first while retaining every assertion. Both hosted mutation proofs pass.
- Root independently reviewed the authored fixtures, public-command/output-recovery checks, typed cases, mutation copy and workflow evidence. Independent integrated implementation/evidence review accepted head `873cfaff0243acb9409a5eda7c16d05ea58b1a46`: the reviewer fetched the source, Windows/Linux job logs and artifact metadata, verified candidate parents/tree, checked the summaries and found no actionable issues. No archive extraction or local run was claimed.

Review is technical evidence; no human GitHub approval or DCO sign-off is fabricated.

## Resume

1. Inspect live PR #5 head, current Windows/Linux native and planning checks, and review conversations.
2. If pending without new failures or actionable changes, stay quiet. If checks fail or review identifies a defect, resolve it in this bounded slice and refresh affected evidence.
3. Await ordinary maintainer integration under the existing contribution policy. No release, paid resource or repository permission change is authorized here.
4. After GitHub verifies an actual merge, record the merge SHA, mark PR-002 done and begin dependency-ready PR-003 from updated main.
