# PR-001 slice plan

- Work item: **PR-001 — Buildable Windows/Linux project shell**; authorized by [the 2026-09-13 instruction](AUTHORIZATION.md).
- Owner: Codex coordinator; branch `codex/pr-001-platform-bootstrap`; base `58ca8f6` on protected `main`. The helper implementation branch is `codex/pr-001-bootstrap-code`, in an isolated worktree.
- Behavior: configure and build a C++20 executable that starts and cleanly stops a deterministic, graphics-free headless fixture through the documented example CLI.
- Scope: CMake/Ninja presets, toolchain pins and diagnostics, native lifecycle and example runner, external success/rejection/recovery tests, Windows/Linux CPU CI, build inventory, instructions and execution handoff. No world command schema, renderer, physics integration, editor, model provider or save format is introduced.
- Prerequisite: the planning foundation is merged. There are no engine-item dependencies.
- Contract: `omniweft_examples --example platform.bootstrap --headless --seed 7 --verify --output artifacts/platform.bootstrap`. Record explicit outcomes and reject unsupported graphics use with an actionable diagnostic. PR-002 will establish the world command schema; this shell does not bypass a world mutation contract because it owns no mutable world state.
- Ownership: implementation agent owns native/build/test/CI files in its worktree; coordinator owns Markdown and planning state; independent architecture reviewer owns no files.
- Independent oracle: an external standard-library Python subprocess harness checks fixed expected lifecycle/artifact values, process exit status and output. Expected values are authored independently of runtime helpers.
- Negative/recovery cases: malformed CLI, graphics requested from a headless-only build, missing build tools and output I/O failure; a valid run after failure must produce a clean successful result. No persistent migration or graphics recovery claim.
- Required evidence: configure/build/CTest on Windows and Ubuntu 24.04, the direct seed-7 example, planning validation and its regression tests. Record exact compiler/tool versions, candidate commit, commands, environment, assertion results, artifact hashes and limitations. Local Windows 11 evidence is separate from hosted Windows Server CI. Real GPU lanes are not applicable to this slice.
- Provenance: first-party generated source uses Apache-2.0. The shell links only the C++ standard library and platform runtime; inventory tool inputs and licenses. SDL/Vulkan/Jolt remain deferred integration dependencies. No network or provider key is required at runtime or by the core tests.
- Review: independent architecture review followed by independent review of the final implementation and evidence; resolve findings before integration.
- Rollback/compatibility: revert through a PR; no existing runtime API or stored worlds exist to migrate. No release or SemVer compatibility claim.
- Risks: toolchain drift, tests passing without a real lifecycle, output corruption and false platform claims. Exact unavailable checks remain not-run. Maintainer merge/certification requirements remain a separate integration gate.
