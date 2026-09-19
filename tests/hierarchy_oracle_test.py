#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""The independent hierarchy oracle must reject plausible corrupted evidence."""
import copy
from hierarchy_oracle import expected_report, validate_report, Failure, CHECKS, ROOT, strict_json


def main():
    schema = strict_json((ROOT / "schemas/protocol-0.1.schema.json").read_bytes())
    assert {"$ref": "#/$defs/reparent"} in schema["properties"]["operations"]["items"]["oneOf"]
    operation = schema["$defs"]["reparent"]
    assert operation["type"] == "object" and operation["additionalProperties"] is False
    assert set(operation["required"]) == set(operation["properties"]) == {"type", "target", "parent", "mode"}
    assert operation["properties"] == {
        "type": {"const": "entity.reparent"},
        "target": {"oneOf": [{"$ref": "#/$defs/temporaryTarget"}, {"$ref": "#/$defs/durableTarget"}]},
        "parent": {"oneOf": [{"type": "null"}, {"$ref": "#/$defs/temporaryTarget"}, {"$ref": "#/$defs/durableTarget"}]},
        "mode": {"enum": ["preserve_world", "preserve_local"]}}
    original = expected_report()
    validate_report(original)
    paths = [
        ("results", 0, "after", "slots", 1, "entity", "transform", "position_m", 0),
        ("results", 0, "after", "slots", 1, "entity", "parent", "generation"),
        ("results", 1, "after", "slots", 1, "entity", "local_transform", "position_m", 0),
        ("results", 2, "receipt", "status"),
        ("results", 2, "after_canonical_hex"),
        ("results", 3, "after", "slots", 1, "entity", "authoring_revision"),
        ("results", 4, "after", "format_version"),
    ]
    cases = []
    for path in paths:
        modified = copy.deepcopy(original)
        target = modified
        for key in path[:-1]: target = target[key]
        target[path[-1]] = "corrupt" if type(target[path[-1]]) is str else target[path[-1]] + 1
        cases.append(modified)
    flag = copy.deepcopy(original); flag["seed"] = True; cases.append(flag)
    extra = copy.deepcopy(original); extra["host_path"] = "unexpected"; cases.append(extra)
    missing = copy.deepcopy(original); missing["results"].pop(); cases.append(missing)
    for modified in cases:
        try: validate_report(modified)
        except Failure: pass
        else: raise RuntimeError("corrupt hierarchy evidence was accepted")
    CHECKS.clear()
    print("hierarchy oracle selftest passed: " + str(len(cases)) + " corruption cases")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
