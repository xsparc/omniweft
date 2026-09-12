# Evidence manifest template

Store runtime results as JSON with at least the following fields. Values below are placeholders, not a completed run.

```json
{
  "schema_version": 1,
  "work_item": "PR-021",
  "candidate_sha": "EXACT_TESTED_COMMIT",
  "example": "pixels.paint_region",
  "timestamp_utc": "RUN_TIMESTAMP",
  "command": "ACTUAL_COMMAND",
  "exit_code": null,
  "environment": {
    "os": "OS_AND_VERSION",
    "compiler": "COMPILER_AND_VERSION",
    "configuration": "BUILD_PRESET",
    "cpu": "MODEL",
    "gpu": null,
    "driver": null
  },
  "seed": 7,
  "lanes": {"cpu": "not_run", "gpu": "not_run"},
  "assertions": [],
  "artifacts": [],
  "limitations": []
}
```

Each assertion records name, expected/actual value and units/tolerance where relevant, plus result. Each artifact records relative path or CI URL, SHA-256 and media/type description. Record real exit codes and timings. Keep secrets and unnecessary personal data out of all artifacts.

The PR includes a short human-readable explanation of what changed, why the example proves it, what ran, what failed or remains untested, and the independent review result. Required unrun lanes prevent a complete feature claim. Retain evidence for the actual tested commit in CI artifacts; record the merge commit separately in the ledger after merge.
