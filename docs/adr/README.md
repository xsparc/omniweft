# Architecture decisions

ADRs record the proposed implementation baseline. They are accepted for this design package; implementation spikes must verify their assumptions before claiming platform support or performance.

| ADR | Decision | Revisit when |
| --- | --- | --- |
| [0001](0001-runtime-stack.md) | C++20 native runtime, SDL3/Vulkan/Jolt, external Python SDK | Build/render/physics spikes show unsustainable cost or missing support |
| [0002](0002-authoritative-transactions.md) | Typed, capability-scoped authoritative transactions | Proven contention requires more granular concurrency |
| [0003](0003-hybrid-representations.md) | Entities plus distinct meshes, sparse block voxels and images | Measured workload supports SDF/LOD/streaming alternatives |
| [0004](0004-license-and-contributions.md) | Apache-2.0 and normal DCO contributions | Community goals or actual legal needs change |

New decisions state context, alternatives, outcome, consequences, verification and reconsideration triggers. Supersede a decision explicitly; preserve the earlier rationale for reviewers.
