# protocol.reject_invalid

Status: **implementation in progress; verification and merge pending**. Work item: **PR-002 — Versioned command schema**.

Dependencies: PR-001. Validation lanes: cpu.

## Behavior and scope

Implement only the named behavior, its public contract, corresponding runnable example and directly required tests/docs. Split the item before execution if it cannot be reviewed as one behavior.

Use a tiny deterministic fixture with seed 7, generated locally or covered by an explicit asset license manifest. Exercise the public command/query interfaces once available, avoiding privileged test-only mutation paths.

## Build and run

Build with the pinned CMake/toolchain instructions in [platform.bootstrap](platform-bootstrap.md). The same headless executable exposes:

```sh
omniweft_examples --example protocol.reject_invalid --headless --seed 7 --verify --output artifacts/protocol.reject_invalid
```

The built-in fixture submits schema-valid and invalid envelopes without creating a world. Supply raw JSON byte fixtures using repeatable `--input <file>` arguments (at most 64) to exercise several independent envelopes in one process. The output directory must be new; existing output is preserved.

`result.json` records `schema_valid` or `schema_invalid` for each input, structured error code/path and valid typed round-trip data with stable serialized bytes. A completed validation run exits zero even when an envelope is rejected; CLI, I/O or self-check failures remain nonzero. `--verify` checks fixture/report consistency, not authorization or world state.

## Contract boundaries

The [schema](../schemas/protocol-0.1.schema.json) and [slice plan](../docs/execution/PR-002-PLAN.md) define the envelope and `entity.create` / `transform.set` variants. The native API checks at most 1 MiB of bytes and 32 containers before constructing a DOM, rejects decoded duplicate keys and unknown fields, and preserves unsigned 64-bit identities, generations, revisions and sequence values exactly. Integer fields reject fractional/exponent syntax. Numeric vectors must be finite binary64 values.

The validator has no world, executor, authentication or capability parameter. Schema success does not establish entity existence, access, epoch admission, temporary-reference resolution or transaction atomicity. PR-003 will apply the retained negative/recovery fixtures to actual world mutation.

## Pass criteria

Round-trip a known valid envelope and return structured validation results for schema 0.1.

## Negative and recovery cases

Malformed, unknown-version, oversized, deeply nested and non-finite input is rejected without world mutation.

Use independently authored expected fixture values and preserve minimized repro inputs when a check fails. If a numerical tolerance is needed, name the fixture, units, timestep/device, threshold and reason before running; do not choose it after seeing a failure.

## Evidence and completion

Record exact candidate SHA, commands, environment, seed, assertions, results, artifact hashes and limitations using docs/templates/EVIDENCE.md. See the [evidence template](../docs/templates/EVIDENCE.md). A screenshot alone cannot pass the feature. Missing required lanes are `not_run`, and prevent a complete claim for that lane.

Revert the PR; preserve source fixtures and earlier save data. Any persistent schema change needs a versioned migration and recovery fixture before merge.

The independent Python oracle checks field values and rejection outcomes using its own fixtures; the native fixture directly checks typed non-finite serialization. Hosted Windows/Linux jobs retain candidate, command, assertion and artifact evidence. [The handoff](../docs/execution/PR-002-HANDOFF.md) distinguishes observed results from pending checks; local execution is currently unavailable. The [roadmap](../docs/ROADMAP.md) and [backlog](../planning/backlog.json) retain the same work-item and example IDs.
