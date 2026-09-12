# Risks and decision triggers

| Risk | Consequence | Mitigation and decision trigger |
| --- | --- | --- |
| Scope grows to a complete AAA engine immediately | No usable release | Ship M1 first; only claim the features with examples; defer breadth through future tracks |
| “Every pixel” becomes per-element model calls | Unbounded latency/cost | Addressable resources plus region/bulk operations and bounded compute; profile observation/edit throughput |
| Model output mutates internals | Corruption and privilege bypass | Typed engine-side validation, capability checks, no generated-code eval |
| Mesh/voxel edits outrun cooking | Lag and inconsistent collision | Prepare then publish matching revisions; bounded queues, cancel stale work, measure cooking latency |
| Dynamic concave deformation is assumed solved | Unstable or unsupported collision | Restrict preview to static terrain and primitive/convex dynamic shapes; separate research gate |
| GPU dependencies contaminate the core | Headless and CI become impractical | Build world/commands without graphics/physics/providers; test dependency boundary |
| Determinism overclaim | Misleading research and networking guarantees | Publish tiers; exact authoring hashes, qualified simulation traces, tolerant GPU evidence |
| Async commit/presentation conflation | User sees “done” before visible state | Separate admitted/prepared/committed/presented receipts; revision diagnostics |
| Agent/resource abuse | Frame starvation or memory exhaustion | Egress/allocation/job quotas, fairness, priority for simulation, malformed-session testing |
| Provider process mistaken for sandbox | Host compromise by untrusted code | Treat scripts as trusted unless actual OS isolation exists; constrain network/control scopes |
| Large assets/models have unclear rights | Unredistributable release | Per-item provenance and terms; offline procedural fixtures; dependency/license gate |
| Public PR executes on personal GPU host | Credential or machine compromise | Disposable isolated GPU runners and trusted dispatch; read-only untrusted CI |
| One maintainer is the integration bottleneck | Review backlog | Bound concurrency, small slices, independent agent review evidence, add real reviewers before raising approval count |
| CI lacks real Windows/Linux GPUs | False support claims | Separate CPU progress from GPU gate; block release certification when evidence missing |
| World history retains too many blobs | Storage exhaustion | Quota-aware checkpoints/retention and reference-rooted GC; reject durable edits before corruption |
| C++ memory/ABI complexity | Safety and maintenance failures | RAII, explicit ownership, sanitizer/fuzz lanes, no stable native ABI promise yet |
| Engine choice proves too costly | Endless infrastructure work | Revisit ADR-0001 after build/render/geometry spikes; consider embedding existing engine without hiding tradeoffs |
| Working name conflicts later | Rename and identity cost | Treat initial search as provisional; conduct wider naming/trademark review before broad branding investment |

An unresolved research question is not a failed engineering task. Define a bounded experiment, metric, comparison baseline and decision owner. Change architecture through an ADR with evidence, preserve compatibility where practical and update affected roadmap/example contracts together.
