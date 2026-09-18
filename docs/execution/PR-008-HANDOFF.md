# PR-008 handoff

State: in progress; no PR yet. Owner: Codex coordinator.
Branch: codex/pr-008-ai-blocks. Base: 1c94777a9fac7869cc83e21f01b1ccc034c439a4.
Scope and checks: [slice plan](PR-008-PLAN.md), [authorization](AUTHORIZATION.md).

The PR-007 repair is integrated and all six post-merge checks passed. This branch reconciles its durable records and claims the next dependency-ready item. Independent architecture review completed. The policy-backed example, shared report serialization, additive SDK presentation options, host-owned capture barriers and independent structural/GPU oracle are implemented. A first Windows Release build, actual physical-GPU showcase run and oracle corruption selftests passed on the working tree. Final clean-candidate validation, Linux validation, source/evidence review and draft publication remain pending.

Next: implement the policy-backed room example with an independently authored state/pixel oracle, validate the actual candidate and publish only reviewed, privacy-safe artifacts. No new merge authority or permission change is adopted.

Independent source and oracle review closed with no remaining blocker after correcting the policy runtime presentation serializer and evidence retention/type/provenance checks. Reviewers did not replay tests. Local Linux Docker verification is not_run because its engine is unavailable; use configured hosted Linux checks without changing host trust or spending. Clean retained evidence and final package audit remain pending.
