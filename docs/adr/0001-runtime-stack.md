# ADR-0001: Native runtime with external AI clients

Status: accepted design baseline; implementation verification pending. Date: 2026-09-13.

## Context and decision

The engine needs Windows/Linux graphics, compute, rigid bodies, editable representations, headless tests and provider independence. Choose C++20/CMake, SDL3, Vulkan 1.3, a Jolt adapter and a separate Python SDK. Pin actual compiler/library revisions in PR-001 and verify them in platform examples before promising support.

## Alternatives

| Alternative | Strength | Reason not selected initially |
| --- | --- | --- |
| Rust core + wgpu | Strong memory-safety tools and a useful GPU abstraction | Different physics/graphics integration effort; team capability and bindings need evidence |
| Extend Godot or another mature engine | Existing editor, audio, animation and packaging | Deep versioned geometry/pixel/physics transaction ownership would depend on upstream internals; prototype before choosing a fork |
| C++ with multiple direct graphics backends | Platform-specific control | Duplicates renderer and test effort before proving the core interaction |
| Embedded Python drives engine internals | Easy scripting | Couples interpreter failures/latency and mutable internals to simulation; use external typed IPC first |

These are design tradeoffs, not benchmarks or claims that another engine cannot implement the idea. An embedding prototype remains a valid fallback if native infrastructure costs exceed available capacity.

## Consequences and validation

C++ requires disciplined ownership, sanitizers and review. Vulkan requires explicit lifetime/synchronization work. Jolt is isolated behind engine contracts. Provider execution is optional and out of process; untrusted-code sandboxing remains separate. PR-001, PR-004 and PR-025 are the initial evidence gates. See [architecture](../ARCHITECTURE.md) and [primary sources](../SOURCES.md).
