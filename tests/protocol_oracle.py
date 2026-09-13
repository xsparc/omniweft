#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Independent subprocess oracle for the schema 0.1 command boundary."""
import argparse
import copy
import hashlib
import json
from pathlib import Path
import shutil
import tempfile

import bootstrap_oracle as audit

FIXTURES = Path(__file__).resolve().parent / "fixtures" / "protocol"
MAX_BYTES = 1_048_576
UINT64_MAX = 18_446_744_073_709_551_615


class Evidence(audit.Evidence):
    def __init__(self, executable, directory):
        super().__init__(executable, directory)
        self.manifest.update(
            work_item="PR-002", example="protocol.reject_invalid",
            limitations=["Schema parsing and serialization only; no world, authorization, transaction admission, rendering, or physics validation."])

    def retain(self, source, relative):
        if self.directory:
            target = self.directory / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(source, target)
            self.manifest["artifacts"][relative] = {
                "path": relative, "sha256": hashlib.sha256(target.read_bytes()).hexdigest()}


def encoded(value):
    return json.dumps(value, ensure_ascii=False, separators=(",", ":"), allow_nan=False).encode("utf-8")


def equivalent(actual, expected):
    # JSON numbers share one semantic type, but booleans must never equal 0/1.
    if isinstance(expected, bool) or expected is None:
        return type(actual) is type(expected) and actual == expected
    if isinstance(expected, (int, float)):
        return type(actual) in (int, float) and actual == expected
    if isinstance(expected, dict):
        return isinstance(actual, dict) and actual.keys() == expected.keys() and all(
            equivalent(actual[key], value) for key, value in expected.items())
    if isinstance(expected, list):
        return isinstance(actual, list) and len(actual) == len(expected) and all(
            equivalent(left, right) for left, right in zip(actual, expected))
    return type(actual) is type(expected) and actual == expected


def assert_json(name, actual, expected):
    audit.require(equivalent(actual, expected), name + ": exact JSON values and field set")

def make_cases(valid, expected):
    cases = []
    def accepted(name, value=None, raw=None):
        value = expected if value is None else value
        cases.append((name, encoded(value) if raw is None else raw, value, None, None))
    def rejected(name, raw, code, path=""):
        cases.append((name, raw, None, code, path))
    def changed(name, path, value, code="INVALID_SCHEMA", error_path=None):
        item = copy.deepcopy(expected)
        target = item
        for component in path[:-1]:
            target = target[component]
        target[path[-1]] = value
        rejected(name, encoded(item), code,
                 "/" + "/".join(str(part) for part in path) if error_path is None else error_path)
    compact = encoded(expected)
    accepted("valid-first", raw=valid)
    changed("unknown-version", ("protocol_version",), "0.2", "UNSUPPORTED_VERSION")
    accepted("valid-after-rejection", raw=valid)
    rejected("raw-nul-after-valid", valid+b"\x00", "INVALID_SCHEMA")
    accepted("valid-after-raw-nul", raw=valid)
    rejected("raw-nul-before-garbage", valid+b"\x00unparsed-garbage", "INVALID_SCHEMA")
    accepted("valid-after-nul-garbage", raw=valid)
    for name, raw in [("empty", b""), ("truncated", compact[:-1]), ("trailing-data", compact+b" false"),
                      ("comment", b"//comment\n"+compact),
                      ("trailing-comma", compact[:-1]+b",}"),
                      ("bad-escape", compact.replace(b'"workshop"', b'"bad\\x20"')),
                      ("invalid-utf8", compact.replace(b'"workshop"', b'"\xff"')),
                      ("unpaired-surrogate", compact.replace(b'"workshop"', b'"\\ud800"')),
                      ("unescaped-control", compact.replace(b'"workshop"', b'"bad\nid"'))]:
        rejected(name, raw, "INVALID_SCHEMA")
    for token in (b"NaN", b"Infinity", b"-Infinity"):
        rejected("lexical-" + token.decode("ascii").replace("-", "minus"),
                 compact.replace(b'"expected_world_revision":42',
                                 b'"expected_world_revision":'+token), "INVALID_SCHEMA")
    for token in (b"1e309", b"-1e309"):
        rejected("overflow-" + token.decode("ascii").replace("-", "minus"),
                 compact.replace(b'"expected_world_revision":42',
                                 b'"expected_world_revision":'+token), "NONFINITE_VALUE")
    for token in (b"null", b"[]", b"true", b"7", b'"text"'):
        rejected("root-" + token.decode().replace('"', ""), token, "INVALID_SCHEMA")
    for field in expected:
        missing = copy.deepcopy(expected)
        del missing[field]
        rejected("missing-" + field, encoded(missing), "INVALID_SCHEMA", "/"+field)
    changed("principal-not-an-authority", ("principal",), "administrator")
    changed("unknown-envelope-field", ("unexpected",), 1)
    changed("unknown-idempotency-field", ("idempotency", "unexpected"), 1)
    changed("unknown-apply-field", ("apply_at", "unexpected"), 1)
    changed("unknown-budget-field", ("budget", "unexpected"), 1)
    changed("unknown-operation-field", ("operations", 0, "unexpected"), 1)
    changed("unknown-target-field", ("operations", 1, "target", "unexpected"), 1)
    changed("unknown-operation", ("operations", 0, "type"), "entity.execute", "UNSUPPORTED_OPERATION")
    changed("operations-not-array", ("operations",), {})
    changed("operation-not-object", ("operations", 0), 0)
    changed("position-wrong-length", ("operations", 1, "position_m"), [0, 2])
    changed("rotation-wrong-length", ("operations", 1, "rotation_xyzw"), [0, 0, 1])
    changed("scale-wrong-length", ("operations", 1, "scale"), [1, 1])
    changed("position-boolean", ("operations", 1, "position_m"), [True, 2, 0],
            error_path="/operations/1/position_m/0")
    changed("position-string", ("operations", 1, "position_m"), ["0", 2, 0],
            error_path="/operations/1/position_m/0")
    for suffix, prefix in [("root", b'{"world_id":"other",'),
                           ("escaped-root", b'{"world\\u005fid":"other",')]:
        rejected("duplicate-" + suffix, prefix + compact[1:], "INVALID_SCHEMA")
    rejected("duplicate-nested", compact.replace(b'"sequence":8', b'"sequence":8,"sequence":9'), "INVALID_SCHEMA")
    rejected("duplicate-escaped-nested", compact.replace(b'"sequence":8', b'"sequence":8,"sequen\\u0063e":9'), "INVALID_SCHEMA")
    accepted("exact-byte-limit", raw=compact+b" "*(MAX_BYTES-len(compact)))
    rejected("over-byte-limit", compact+b" "*(MAX_BYTES+1-len(compact)), "BUDGET_EXCEEDED")
    # Container depth includes the outer object; schema rejects the unknown key only after the bounded parse.
    for depth, code, path in [(32, "INVALID_SCHEMA", "/unexpected"), (33, "BUDGET_EXCEEDED", "")]:
        raw = compact[:-1] + b',"unexpected":' + b"["*(depth-1) + b"0" + b"]"*(depth-1) + b"}"
        rejected("container-depth-" + str(depth), raw, code, path)

    # U64 values stay exact beyond the binary64 integer precision boundary.
    for label, number in [("u64-zero", 0), ("u64-above-double-integer", 9_007_199_254_740_993),
                          ("u64-maximum", UINT64_MAX)]:
        item = copy.deepcopy(expected)
        item["expected_world_revision"] = number
        item["idempotency"]["sequence"] = number
        item["budget"]["max_blob_bytes"] = number
        item["operations"][1]["target"] = {
            "world_id": "another-world", "entity_uuid": "ffffffff-ffff-ffff-ffff-ffffffffffff",
            "generation": number}
        accepted(label, item)
    integer_paths = [("expected_world_revision",), ("idempotency", "sequence"),
                     ("budget", "max_blob_bytes"), ("apply_at", "expires_after_ticks"),
                     ("budget", "max_operations")]
    for path in integer_paths:
        label = "-".join(path)
        for kind, value in [("boolean", True), ("null", None), ("string", "1"),
                            ("negative", -1), ("fraction", 1.5)]:
            changed(label+"-"+kind, path, value)
        for kind, token in [("decimal", b"1.0"), ("exponent", b"1e0"),
                            ("u64-overflow", b"18446744073709551616")]:
            item = copy.deepcopy(expected)
            target = item
            for key in path[:-1]:
                target = target[key]
            target[path[-1]] = 18_446_744_073_709_550_000
            rejected(label+"-"+kind,
                     encoded(item).replace(b"18446744073709550000", token),
                     "INVALID_SCHEMA", "/"+"/".join(path))
    changed("expiry-zero", ("apply_at", "expires_after_ticks"), 0)
    changed("expiry-overflow", ("apply_at", "expires_after_ticks"), 4_294_967_296)
    expiry = copy.deepcopy(expected)
    expiry["apply_at"]["expires_after_ticks"] = 4_294_967_295
    accepted("expiry-maximum", expiry)
    changed("operation-budget-zero", ("budget", "max_operations"), 0, "BUDGET_EXCEEDED")
    changed("operation-budget-over-limit", ("budget", "max_operations"), 257, "BUDGET_EXCEEDED")
    changed("operation-budget-under-count", ("budget", "max_operations"), 1,
            "BUDGET_EXCEEDED", "/operations")
    generous = copy.deepcopy(expected)
    generous["budget"]["max_operations"] = 3
    accepted("operation-budget-is-maximum", generous)
    changed("empty-operations", ("operations",), [], "BUDGET_EXCEEDED")
    for count in (256, 257):
        item = copy.deepcopy(expected)
        item["budget"]["max_operations"] = 256
        item["operations"] = [{"type": "entity.create", "temporary_id": "entity-"+str(index),
                               "prefab": "builtin.unit_cube"} for index in range(count)]
        if count == 256:
            accepted("operation-limit-256", item)
        else:
            rejected("operation-limit-257", encoded(item), "BUDGET_EXCEEDED", "/operations")
    for path, maximum in [(("world_id",), 128), (("idempotency", "epoch"), 128),
                          (("operations", 0, "prefab"), 128), (("operations", 0, "temporary_id"), 64)]:
        label = "-".join(str(key) for key in path)
        for kind, value in [("empty", ""), ("long", "a"*(maximum+1)), ("invalid-first", ":name"),
                            ("unicode", "caf\u00e9"), ("control", "a\u0000b")]:
            changed(label+"-"+kind, path, value)
        item = copy.deepcopy(expected)
        target = item
        for key in path[:-1]:
            target = target[key]
        target[path[-1]] = "a"*maximum
        accepted(label+"-maximum", item)
    changed("uuid-uppercase", ("transaction_id",), "018F7242-4387-7C98-A114-67787915A369")
    changed("uuid-malformed", ("transaction_id",), "not-a-uuid")
    changed("version-wrong-type", ("protocol_version",), 0.1)
    changed("operation-type-wrong-type", ("operations", 0, "type"), 1)
    changed("apply-mode-unknown", ("apply_at", "mode"), "immediate")
    changed("target-mixes-forms", ("operations", 1, "target"),
            {"temporary_id": "block", "world_id": "workshop",
             "entity_uuid": "ffffffff-ffff-ffff-ffff-ffffffffffff", "generation": 0},
            error_path="/operations/1/target*")
    accepted("escaped-identifier", raw=compact.replace(b'"workshop"', b'"w\\u006frkshop"'))
    finite_edges = copy.deepcopy(expected)
    finite_edges["operations"][1]["position_m"] = [-1.7976931348623157e308, 0.0, 1.7976931348623157e308]
    finite_edges["operations"][1]["rotation_xyzw"] = [0.0, 0.0, 0.0, 0.0]
    finite_edges["operations"][1]["scale"] = [-1.0, 0.0, 1.0]
    accepted("finite-limits-execution-invariants-deferred", finite_edges)
    durable = copy.deepcopy(expected)
    durable["operations"][1]["target"] = {
        "world_id": "workshop", "entity_uuid": "ffffffff-ffff-ffff-ffff-ffffffffffff", "generation": 0}
    for label, value in [("boolean", True), ("null", None), ("string", "1"),
                         ("negative", -1), ("fraction", 1.5), ("overflow", UINT64_MAX+1)]:
        item = copy.deepcopy(durable)
        item["operations"][1]["target"]["generation"] = value
        rejected("generation-"+label, encoded(item), "INVALID_SCHEMA", "/operations/1/target/generation")
    for label, token in [("decimal", b"1.0"), ("exponent", b"1e0")]:
        rejected("generation-"+label, encoded(durable).replace(b'"generation":0', b'"generation":'+token),
                 "INVALID_SCHEMA", "/operations/1/target/generation")
    deletion = copy.deepcopy(expected)
    deletion["budget"]["max_operations"] = 1
    deletion["operations"] = [{"type": "entity.delete", "target": {"temporary_id": "block"},
                               "child_policy": "reject_if_children"}]
    accepted("delete-temporary-target-roundtrip", deletion)
    durable_deletion = copy.deepcopy(deletion)
    durable_deletion["operations"][0]["target"] = {
        "world_id": "another-world", "entity_uuid": "ffffffff-ffff-ffff-ffff-ffffffffffff",
        "generation": UINT64_MAX}
    accepted("delete-durable-target-roundtrip", durable_deletion)
    for label, policy in [("recursive", "recursive"), ("reparent", "reparent"), ("boolean", True)]:
        item = copy.deepcopy(deletion)
        item["operations"][0]["child_policy"] = policy
        rejected("delete-policy-"+label, encoded(item), "INVALID_SCHEMA", "/operations/0/child_policy")
    missing_policy = copy.deepcopy(deletion)
    del missing_policy["operations"][0]["child_policy"]
    rejected("delete-missing-policy", encoded(missing_policy), "INVALID_SCHEMA", "/operations/0/child_policy")
    extra_delete = copy.deepcopy(deletion)
    extra_delete["operations"][0]["unexpected"] = "ignored-fields-are-not-allowed"
    rejected("delete-unknown-field", encoded(extra_delete), "INVALID_SCHEMA", "/operations/0/unexpected")
    mixed_delete = copy.deepcopy(durable_deletion)
    mixed_delete["operations"][0]["target"]["temporary_id"] = "block"
    rejected("delete-mixed-target-forms", encoded(mixed_delete), "INVALID_SCHEMA", "/operations/0/target*")
    return cases

def inspect_report(payload, cases):
    report = json.loads(payload)
    expected_fields = {"schema_version", "example", "protocol_version", "seed", "verified", "results"}
    audit.equal("report fields", sorted(report), sorted(expected_fields))
    for key, value in {"schema_version": 1, "example": "protocol.reject_invalid",
                       "protocol_version": "0.1", "seed": 7, "verified": True}.items():
        assert_json("report " + key, report[key], value)
    audit.equal("one result per input", len(report["results"]), len(cases))
    serializations = []
    for index, ((name, raw, expected, error_code, error_path), result) in enumerate(zip(cases, report["results"])):
        audit.equal(name + ": index", result["index"], index)
        if expected is not None:
            audit.equal(name + ": accepted fields", sorted(result),
                        ["errors", "index", "round_trip", "serialized", "status"])
            audit.equal(name + ": accepted", result["status"], "schema_valid")
            audit.equal(name + ": no errors", result["errors"], [])
            assert_json(name + ": typed round trip", result["round_trip"], expected)
            audit.require(isinstance(result["serialized"], str), name + ": serialized JSON is text")
            assert_json(name + ": serialized values", json.loads(result["serialized"]), expected)
            for key in ("expected_world_revision",):
                audit.require(type(result["round_trip"][key]) is int, name + ": unsigned " + key + " remains integer")
            audit.require(type(result["round_trip"]["idempotency"]["sequence"]) is int,
                          name + ": unsigned sequence remains integer")
            serializations.append((name, result["serialized"], expected))
        else:
            audit.equal(name + ": rejected", result["status"], "schema_invalid")
            audit.equal(name + ": rejected fields", sorted(result), ["errors", "index", "status"])
            audit.equal(name + ": exactly one error", len(result["errors"]), 1)
            error = result["errors"][0]
            audit.require(set(error) in ({"code", "message", "path"}, {"code", "message", "path", "operation_index"}),
                          name + ": structured error fields")
            if "operation_index" in error:
                parts = error["path"].split("/")
                audit.require(len(parts) > 2 and parts[1] == "operations" and parts[2].isdigit(),
                              name + ": operation index identifies an operation path")
                audit.equal(name + ": operation index", error["operation_index"], int(parts[2]))
            audit.equal(name + ": error code", error["code"], error_code)
            if error_path is not None:
                if error_path.endswith("*"):
                    audit.require(error["path"].startswith(error_path[:-1]), name + ": error path prefix")
                else:
                    audit.equal(name + ": error path", error["path"], error_path)
            audit.require(isinstance(error["path"], str) and
                          (error["path"] == "" or error["path"].startswith("/")),
                          name + ": JSON Pointer error path")
            audit.require(isinstance(error["message"], str) and bool(error["message"].strip()),
                          name + ": actionable error message")
    return serializations


def run_cases(executable, directory, cases, evidence, group):
    inputs = directory / (group + "-inputs")
    inputs.mkdir()
    arguments = [str(executable), "--example", "protocol.reject_invalid",
                 "--headless", "--seed", "7", "--verify", "--output", str(directory / group)]
    for index, (name, raw, expected, error_code, error_path) in enumerate(cases):
        path = inputs / (str(index).zfill(3) + "-" + name + ".json")
        path.write_bytes(raw)
        evidence.retain(path, group + "-inputs/" + path.name)
        arguments.extend(["--input", str(path)])
    process = audit.execute(arguments)
    audit.equal(group + ": example exit code", process.returncode, 0)
    result_path = directory / group / "result.json"
    payload = result_path.read_bytes()
    if group == "cases-0":
        evidence.retain_result(payload)
    else:
        evidence.retain(result_path, group + "-result.json")
    return inspect_report(payload, cases)



def check_public_example(executable, directory, expected, evidence):
    base = [str(executable), "--example", "protocol.reject_invalid",
            "--headless", "--seed", "7", "--verify"]
    output = directory / "public-example"
    command = [*base, "--output", str(output)]
    process = audit.execute(command)
    audit.equal("public example without --input exits zero", process.returncode, 0)
    result_path = output / "result.json"
    payload = result_path.read_bytes()
    evidence.retain(result_path, "builtin-result.json")
    cases = [
        ("builtin-valid-first", b"", expected, None, None),
        ("builtin-invalid-version", b"", None, "UNSUPPORTED_VERSION", "/protocol_version"),
        ("builtin-valid-after-rejection", b"", expected, None, None),
    ]
    values = inspect_report(payload, cases)
    audit.equal("public example recovers with identical serialization", values[0][1], values[1][1])
    collision = audit.execute(command)
    audit.require(collision.returncode != 0, "protocol output collision must fail")
    audit.require("fresh --output directory" in collision.stderr,
                  "protocol output collision gives fresh-directory guidance")
    audit.require(result_path.read_bytes() == payload, "protocol output collision preserves prior result bytes")
    audit.equal("protocol output collision leaves only original result",
                sorted(path.name for path in output.iterdir()), ["result.json"])

    missing_output = directory / "missing-input-output"
    missing_input = directory / "absent-input.json"
    missing = audit.execute([*base, "--output", str(missing_output), "--input", str(missing_input)])
    audit.require(missing.returncode != 0, "missing protocol input must fail")
    audit.require("input_error:" in missing.stderr and "readable JSON fixture" in missing.stderr,
                  "missing protocol input gives readable-fixture guidance")
    audit.require(not missing_output.exists(), "missing protocol input must not publish output")

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", required=True, type=Path)
    parser.add_argument("--evidence-dir", type=Path)
    parser.add_argument("--case-set", choices=("all", "version"), default="all", help="Version-only set is used by the deliberate mutation proof")
    args = parser.parse_args()
    executable = args.executable.resolve(strict=True)
    expected = json.loads((FIXTURES / "expected-envelope.json").read_text(encoding="utf-8"))
    valid = (FIXTURES / "valid-envelope.json").read_bytes()
    # The committed expected values are independent from the native implementation.
    assert_json("fixture matches documented envelope", json.loads(valid), expected)
    with Evidence(executable, args.evidence_dir) as evidence, tempfile.TemporaryDirectory(prefix="ow-protocol-") as temp:
        directory = Path(temp)
        cases = make_cases(valid, expected)
        if args.case_set == "version":
            cases = cases[:3]
        serializations = []
        for offset in range(0, len(cases), 64):
            serializations.extend(run_cases(executable, directory, cases[offset:offset+64],
                                            evidence, "cases-" + str(offset // 64)))
        stable_cases = [(name, text.encode("utf-8"), value, None, None)
                        for name, text, value in serializations]
        second = []
        for offset in range(0, len(stable_cases), 64):
            second.extend(run_cases(executable, directory, stable_cases[offset:offset+64],
                                    evidence, "roundtrip-" + str(offset // 64)))
        audit.equal("stable serialization count", len(second), len(serializations))
        for (name, first, _), (_, subsequent, _) in zip(serializations, second):
            audit.equal(name + ": parse serialize parse is byte stable", subsequent, first)
        if args.case_set == "all":
            check_public_example(executable, directory, expected, evidence)
            input_path = directory / "cases-0-inputs" / "000-valid-first.json"
            output_path = directory / "cli-too-many-inputs"
            command = [str(executable), "--example", "protocol.reject_invalid", "--headless",
                       "--seed", "7", "--verify", "--output", str(output_path)]
            command.extend(arg for _ in range(65) for arg in ("--input", str(input_path)))
            excessive = audit.execute(command)
            audit.require(excessive.returncode != 0, "65 CLI inputs must fail")
            audit.require("64" in excessive.stderr, "CLI input limit has actionable diagnostic")
            audit.require(not output_path.exists(), "rejected CLI must not publish output")
        # The first three cases are valid/invalid/valid, in one native process.
        audit.equal("recovery preserves accepted serialization", serializations[0][1], serializations[1][1])
        print(json.dumps({"status": "passed", "case_count": len(cases),
                          "roundtrip_count": len(second), "builtin_case_count": 3 if args.case_set == "all" else 0,
                          "assertion_count": len(audit.ASSERTIONS)}))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
