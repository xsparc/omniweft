# ADR-0004: Apache-2.0 and contribution certification

Status: accepted for first-party bootstrap material. Date: 2026-09-13.

## Decision

Use Apache-2.0 for first-party code, SDKs, tools, documentation and example fixtures, with explicitly separate third-party content terms. Use DCO 1.1 for normal contributions rather than a bespoke CLA. The initial AI-authored design import does not invent a human sign-off.

## Alternatives and consequences

MIT is simpler but lacks an express patent clause in its text. MPL-2.0 would require source availability for distributed covered-file modifications and better fits a priority of shared engine improvements. Apache favors broad reuse, including closed commercial integrations, with its defined patent/notice conditions. The engine license does not license third-party models/assets or guarantee generated-content ownership.

The [license policy](../OPEN_SOURCE.md) links authoritative texts and defines provenance requirements. A future change needs public governance review and valid rights from contributors; the initial choice does not authorize unilateral relicensing of their work.
