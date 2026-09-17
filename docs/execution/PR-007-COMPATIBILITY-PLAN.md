# PR-007 compatibility follow-up

Base: d7b52696edd3e7cb9c3388972509a05f16dc6466, the actual maintainer squash merge of PR #11. All six PR checks passed; the post-merge Windows native job failed agents.sdk_compatibility while the other five post-merge checks passed. PR-008 stays proposed until this regression is handled.

The child SDK probe was reproduced locally: a retired client submitted after renewal and received OutcomeUnknown instead of the known 401/NOT_AUTHORIZED. A split-header/body reproduction failed five of five attempts with connection abortion and no world mutation. Authentication rejected after headers, queued its response and immediately closed before the SDK's separately written body arrived.

Bounded correction: only the legacy native control host's rejection path half-closes its send side after responding, then discards into a fixed 4096-byte scratch buffer until EOF/reset, the original request/host deadline, or 1 MiB. Discarded bytes never enter parsing/admission and cannot consume authoring sequence or modify state. The policy host's lease/admission path is unchanged. No blocking linger, retry, timeout extension, authorization weakening or SDK error reinterpretation.

Validation: unchanged baseline must fail the new public-SDK split-body regression; corrected control and fixed-step hosts must return exact typed rejection, preserve complete world state and permit fresh-client recovery. An idle rejected peer must not keep the legacy gateway occupied beyond its original request deadline. Run Windows/Linux CPU suites, original policy/authentication/Owner regressions and independent review; retain exact hashes and public-safe evidence. GPU behavior is unchanged. Hosted required checks and maintainer squash merge remain required.

The staged transport close follows [RFC 9112 section 9.6](https://www.rfc-editor.org/rfc/rfc9112.html#section-9.6) and [Microsoft Winsock shutdown guidance](https://learn.microsoft.com/en-us/windows/win32/winsock/graceful-shutdown-linger-options-and-socket-closure-2).
