#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Verify private evidence and export a strictly allowlisted public summary."""
import argparse
import hashlib
import json
import math
from pathlib import Path, PurePosixPath
import re
import sys

SHA = re.compile(r"[0-9a-f]{64}")
SAFE_PATH = re.compile(r"[A-Za-z0-9_. /-]+")
NUMBER_VERSION = re.compile(r"[0-9]+(?:\.[0-9]+){1,4}")
STATUSES = {"passed", "failed", "not_run", "not_applicable", "unsupported"}
PROOFS = {"deliberate_version_validator_mutation", "deliberate_atomic_rollback_mutation", "deliberate_presentation_transform_mutation"}
EXAMPLES = {"platform.bootstrap", "protocol.reject_invalid", "objects.atomic", "render.world_cube"}
TOOLS = {"git", "python", "python.exe", "cmake", "cmake.exe", "ninja", "ninja.exe",
         "omniweft_examples", "omniweft_examples.exe"}
FLAGS = {"--example", "--headless", "--gpu", "--seed", "--verify", "--output", "--input",
         "--help", "--version", "--max-slots", "rev-parse", "HEAD", "status", "--porcelain"}
BOOTSTRAP = {"schema_version": 1, "example": "platform.bootstrap", "mode": "headless", "seed": 7,
             "lifecycle": ["created", "running", "stopped"], "steps": 1,
             "fixture_checksum": 1282168116, "verified": True}


def digest(data):
    return hashlib.sha256(data).hexdigest()


def reject(message):
    raise ValueError(message)


def encoded(value):
    return json.dumps(value, sort_keys=True, separators=(",", ":"), allow_nan=False).encode()


def public_value(value):
    # Only numerical/boolean expectations are safe without a domain-specific schema.
    if value is None or type(value) in (bool, int):
        return value
    if type(value) is float:
        if not math.isfinite(value):
            reject("nonfinite expectation")
        return value
    return {"redacted": True}


def relative_path(value):
    if not isinstance(value, str) or "\\" in value or not SAFE_PATH.fullmatch(value):
        reject("unsafe artifact path")
    p = PurePosixPath(value)
    if p.is_absolute() or any(part in {"", ".", ".."} for part in value.split("/")):
        reject("unsafe artifact path")
    return p


def load_json(payload):
    def unique(pairs):
        result = {}
        for key, value in pairs:
            if key in result:
                reject("duplicate JSON field")
            result[key] = value
        return result
    def finite(value):
        if type(value) is float and not math.isfinite(value):
            reject("nonfinite JSON")
        if isinstance(value, dict):
            for entry in value.values():
                finite(entry)
        if isinstance(value, list):
            for entry in value:
                finite(entry)
    result = json.loads(payload, object_pairs_hook=unique,
                        parse_constant=lambda _: reject("nonfinite JSON"))
    finite(result)
    return result


def export(source, destination, repo, executable, candidate, private_terms=()):
    # private_terms retained for callers/tests; no user strings are emitted at all.
    del private_terms
    source, repo, executable = (p.resolve(strict=True) for p in (source, repo, executable))
    destination = destination.resolve()
    if destination.exists():
        reject("public output must be new")
    if destination == source or source in destination.parents or destination in source.parents:
        reject("private and public directories must be separate")
    raw = (source / "manifest.json").read_bytes()
    record = load_json(raw)
    if not isinstance(record, dict) or type(record.get("schema_version")) is not int or record["schema_version"] != 1 or record.get("candidate_sha") != candidate:
        reject("evidence candidate or schema mismatch")
    if re.fullmatch(r"[0-9a-f]{40}", candidate) is None:
        reject("invalid candidate")
    if record.get("candidate_worktree_dirty") is not False:
        reject("public evidence requires a clean candidate")
    if record.get("status") not in {"passed", "failed"}:
        reject("missing evidence status")
    if type(record.get("exit_code")) is not int or (record["exit_code"] == 0) != (record["status"] == "passed"):
        reject("invalid evidence exit code")
    if record.get("example") not in EXAMPLES or not re.fullmatch(r"PR-0(?:0[1-9]|[12][0-9]|3[0-8])", str(record.get("work_item"))):
        reject("unexpected evidence identity")
    if type(record.get("seed")) is not int or not 0 <= record["seed"] <= 0xFFFFFFFF:
        reject("invalid seed")
    if record.get("lane") not in {"cpu", "gpu"}:
        reject("unexpected lane")
    lanes = record.get("lanes")
    if not isinstance(lanes, dict) or not lanes or any(k not in {"cpu", "gpu", "manual", "benchmark", "optional-provider"}
                                                    or v not in STATUSES for k, v in lanes.items()):
        reject("invalid lane results")
    proof = record.get("proof")
    if proof is not None and proof not in PROOFS:
        reject("unknown mutation proof")
    assertions, commands = record.get("assertions"), record.get("commands")
    if not isinstance(assertions, list) or not assertions or not isinstance(commands, list) or not commands:
        reject("missing assertions or commands")
    for number, assertion in enumerate(assertions, 1):
        if not isinstance(assertion, dict):
            reject("invalid assertion record")
        if type(assertion.get("id")) is not int or assertion["id"] != number or assertion.get("status") not in {"passed", "failed"}:
            reject("invalid assertion sequence")
        if not isinstance(assertion.get("assertion"), str):
            reject("invalid assertion label")
    if record["status"] == "passed" and any(a["status"] != "passed" for a in assertions):
        reject("passing evidence contains failed assertions")
    artifacts = record.get("artifacts")
    if not isinstance(artifacts, dict) or "executable" not in artifacts or "result" not in artifacts:
        reject("missing artifact inventory")
    verified, bootstrap_bytes = [], None
    for artifact_id, (key, artifact) in enumerate(artifacts.items(), 1):
        if not isinstance(artifact, dict) or not SHA.fullmatch(str(artifact.get("sha256", ""))):
            reject("invalid artifact digest")
        value = artifact.get("path")
        if key == "executable":
            if Path(value).resolve() != executable:
                reject("executable does not match explicitly selected binary")
            original = executable
        else:
            rel = relative_path(value)
            original = source.joinpath(*rel.parts)
            if any(p.is_symlink() for p in (original, *original.parents) if p != source.parent):
                reject("symlinks are not public evidence inputs")
            original = original.resolve(strict=True)
            if source not in original.parents:
                reject("artifact escapes evidence directory")
        payload = original.read_bytes()
        if digest(payload) != artifact["sha256"]:
            reject("artifact hash mismatch")
        item = {"id": artifact_id, "role": key if key in {"executable", "result"} else "private_artifact", "sha256": artifact["sha256"],
                "bytes": len(payload), "published": False}
        # This is the ONLY copied artifact schema. Everything else remains a hash
        # commitment. Render reports/readbacks use their dedicated public oracle.
        if key == "result" and record["work_item"] == "PR-001" and record["example"] == "platform.bootstrap":
            result = load_json(payload)
            if result != BOOTSTRAP or any(type(result[k]) is not type(v) for k, v in BOOTSTRAP.items()):
                reject("unexpected bootstrap result schema")
            bootstrap_bytes = payload
            item.update(path="result.json", published=True)
        verified.append(item)
    env, public_env = record.get("environment", {}), {}
    if not isinstance(env, dict):
        reject("invalid environment record")
    for key in ("python", "cmake"):
        value = env.get(key)
        if isinstance(value, str) and NUMBER_VERSION.fullmatch(value):
            public_env[key] = value
    if env.get("ninja") == "1.13.2.git.kitware.jobserver-pipe-1":
        public_env["ninja"] = env["ninja"]
    for key, allowed in (("configuration", {"Debug", "Release", "RelWithDebInfo", "MinSizeRel"}),
                         ("generator", {"Ninja"}), ("machine", {"AMD64", "x86_64", "arm64", "aarch64"})):
        if env.get(key) in allowed:
            public_env[key] = env[key]
    compiler = env.get("compiler", {})
    if not isinstance(compiler, dict):
        reject("invalid compiler record")
    public_env["compiler"] = {}
    if compiler.get("CMAKE_CXX_COMPILER_ID") in {"MSVC", "Clang", "GNU", "AppleClang"}:
        public_env["compiler"]["id"] = compiler["CMAKE_CXX_COMPILER_ID"]
    version = compiler.get("CMAKE_CXX_COMPILER_VERSION")
    if isinstance(version, str) and NUMBER_VERSION.fullmatch(version):
        public_env["compiler"]["version"] = version
    public_commands = []
    for cmd in commands:
        if not isinstance(cmd, dict):
            reject("invalid command record")
        argv = cmd.get("command")
        if not isinstance(argv, list) or not argv or any(not isinstance(a, str) for a in argv):
            reject("invalid command record")
        if cmd.get("exit_code") is not None and type(cmd["exit_code"]) is not int:
            reject("invalid command exit code")
        tool = argv[0].replace("\\", "/").rsplit("/", 1)[-1]
        public_commands.append({
            "tool": tool if tool in TOOLS else "private-tool",
            "arguments": [v if v in FLAGS else public_value(v) for v in argv[1:]],
            "exit_code": cmd.get("exit_code"),
        })
    public = {
        "schema_version": 1, "format": "omniweft.public-evidence.v1",
        "raw_manifest_sha256": digest(raw), "candidate_sha": candidate,
        "candidate_worktree_dirty": False, "status": record["status"], "exit_code": record["exit_code"],
        "work_item": record["work_item"], "example": record["example"],
        "evidence_kind": "mutation_detection" if proof else "feature_check", "proof": proof,
        "qualification": "A passing mutation proof means the independent oracle rejected the deliberately changed implementation." if proof else "Feature-check outcomes; read lane results and omitted-evidence limitations.",
        "seed": record["seed"], "lane": record["lane"], "lanes": lanes,
        "assertion_count": len(assertions), "command_count": len(commands), "artifact_count": len(verified),
        "assertions": [{"id": a["id"],
                        "expected": public_value(a.get("expected")), "actual": public_value(a.get("actual")),
                        "status": a["status"]} for a in assertions],
        "commands": public_commands, "environment": public_env, "artifacts": verified,
        "limitations": ["Free-form strings, compound expectations, paths, logs and binaries are omitted; the whole raw manifest is hash-bound.",
                        "Assertion IDs, outcomes, finite numerical expectations, command exit codes and lane results are retained.",
                        "Only the exact known bootstrap result schema is copied; other artifacts require their domain-specific public exporter."],
    }
    payload = (json.dumps(public, indent=2, allow_nan=False) + "\n").encode()
    destination.mkdir(parents=True, exist_ok=False)
    if bootstrap_bytes is not None:
        (destination / "result.json").write_bytes(bootstrap_bytes)
    (destination / "manifest.json").write_bytes(payload)
    (destination / "manifest.sha256").write_text(digest(payload) + "\n", encoding="ascii")
    return public


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--repo", type=Path, required=True)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--candidate", required=True)
    args = parser.parse_args()
    result = export(args.source, args.output, args.repo, args.executable, args.candidate)
    print(json.dumps({key: result[key] for key in ("status", "candidate_sha", "assertion_count", "command_count")}))
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (ValueError, OSError, KeyError, TypeError):
        print("public_evidence_export_failed: invalid or private input; no raw details printed", file=sys.stderr)
        raise SystemExit(1)
