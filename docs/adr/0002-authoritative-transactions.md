# ADR-0002: Typed authoritative transactions

Status: accepted design baseline. Date: 2026-09-13.

## Decision

All human/AI world mutations use typed operations, engine-side capabilities, bounded resources, explicit revision preconditions and commit receipts. Use one coordinator initially. Authoring revision, simulation tick and presentation frame are separate. Idempotency uses server-issued epochs and monotonic client sequences; durability has an explicit watermark.

## Alternatives and consequences

Direct mutable pointers are fast to prototype but cannot support remote clients, uniform validation or reliable replay. Silent last-writer-wins editing loses intent under concurrent authors. Making every world-building plan globally atomic can require unbounded preparation/memory; use atomic bounded batches and explicit multi-step plans instead.

Transactions add validation, staging and logging cost. They make failures, recovery and provenance observable. Long GPU/collider work prepares off-thread and revalidates authored resources before publication. API-specific read/write budgets are mandatory. Headless operation never needs a renderer acknowledgement.

## Verification and revisit trigger

PR-003, PR-007, PR-011 and PR-014 demonstrate atomicity, policy, duplicate/conflicting requests and crash durability. PR-028 proves physical geometry publication. Revisit global authoring preconditions when contention measurements justify resource-level read/write sets; preserve deterministic journal order and explicit conflicts.
