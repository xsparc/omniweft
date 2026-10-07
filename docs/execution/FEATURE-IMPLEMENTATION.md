# Adopted feature implementation

On 2026-10-07 the maintainer explicitly requested: “Please continue and implement the proposals.” This adopts incremental implementation of FP-001 through FP-040 in the four reviewed catalogs. The new instruction supplies implementation authority; the earlier documentation merges alone did not. The [authorization record](AUTHORIZATION.md) preserves the existing contribution, privacy and manual integration boundaries.

The adopted contracts are the bounded first slices, prerequisites, success oracles, rejection and recovery criteria in [FP-001–010](../FEATURE_PROPOSALS.md), [FP-011–020](../FEATURE_PROPOSALS_2.md), [FP-021–030](../FEATURE_PROPOSALS_3.md) and [FP-031–040](../FEATURE_PROPOSALS_4.md). Broader product aspirations and exclusions in those documents are not silently promoted into a single implementation. None is complete merely because it is authorized.

The original PR-001–038 roadmap remains adopted with its dependencies and release gates unchanged. FP items supplement that work. The coordinator selects one dependency-ready behavior, refines its plan and example, then adds that executable slice to the single [backlog](../../planning/backlog.json). Unclaimed adopted proposals remain awaiting refinement; they do not need a second competing execution ledger. If an FP prerequisite needs an original roadmap item, implement that dependency first. Neither authorization nor a proposal merge bypasses the dependency gate.

First selected slice: **FP-008**, the read-only asset retention inspector, because PR-016's complete Catalog contract is already integrated. Its [plan](FP-008-PLAN.md) and [handoff](FP-008-HANDOFF.md) track actual implementation and evidence. Every subsequent slice still requires a runnable example, independent success/rejection/recovery oracle, Windows/Linux verification, independent review, a draft PR and manual maintainer integration before completion.

## Adopted catalog inventory

| Catalog | Adopted identifiers |
| --- | --- |
| [First](../FEATURE_PROPOSALS.md) | FP-001, FP-002, FP-003, FP-004, FP-005, FP-006, FP-007, FP-008, FP-009, FP-010 |
| [Second](../FEATURE_PROPOSALS_2.md) | FP-011, FP-012, FP-013, FP-014, FP-015, FP-016, FP-017, FP-018, FP-019, FP-020 |
| [Third](../FEATURE_PROPOSALS_3.md) | FP-021, FP-022, FP-023, FP-024, FP-025, FP-026, FP-027, FP-028, FP-029, FP-030 |
| [Fourth](../FEATURE_PROPOSALS_4.md) | FP-031, FP-032, FP-033, FP-034, FP-035, FP-036, FP-037, FP-038, FP-039, FP-040 |

## Verified proposal integration

PR #29 merged as `2afd15d1cc827389e084bc9c8223f49a7148aded`, tree `88b27c3687b029e02f229c779c07d26849dd3314`, matching reviewed delivery `1709e19e7bfe54e10830de6ef6b93ab22b3eaa3b`. All six actual-merge checks passed: [native](https://github.com/xsparc/omniweft/actions/runs/37599858626), [renderer](https://github.com/xsparc/omniweft/actions/runs/37599858584), [planning](https://github.com/xsparc/omniweft/actions/runs/37599858588). Both native manifests use the actual merge SHA/tree, with 2340 GLB oracle and 4090 native assertions each; all 74 source, 16 SDK and three artifact bindings were verified. Debug compilers were Windows MSVC 19.44.35229.0 and Linux Clang 18.1.3. These are existing engine regression results, not implementation evidence for any FP feature.

This record supersedes historical catalog/handoff statements that proposals remain unadopted or that PR #29 awaits merge. Original proposal and implementation history remain available.
