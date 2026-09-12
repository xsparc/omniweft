#!/usr/bin/env python3
"""Validate the planning repository; this does not test an engine runtime."""

from __future__ import annotations

import json
import re
import sys
from pathlib import Path
from urllib.parse import unquote, urlsplit


ROOT = Path(__file__).resolve().parents[1]
REQUIRED = (
    "README.md", "LICENSE", "NOTICE", "CONTRIBUTING.md", "AGENTS.md",
    "CODE_OF_CONDUCT.md", "GOVERNANCE.md", "MAINTAINERS.md", "SECURITY.md",
    "SUPPORT.md", "THIRD_PARTY.md", "CHANGELOG.md", "planning/backlog.json",
    "docs/PROJECT_BRIEF.md", "docs/ARCHITECTURE.md", "docs/WORLD_MODEL.md",
    "docs/AI_CONTROL_PROTOCOL.md", "docs/SIMULATION_AND_RENDERING.md",
    "docs/EDITOR_AND_WORKFLOWS.md", "docs/ROADMAP.md", "docs/VALIDATION.md",
    "docs/AUTONOMOUS_DEVELOPMENT.md", "docs/OPEN_SOURCE.md", "docs/RISKS.md",
    "docs/FUTURE_TRACKS.md", "docs/SOURCES.md", "docs/REPOSITORY_SETUP.md",
    "docs/BOOTSTRAP_VERIFICATION.md", "docs/adr/README.md", "examples/README.md",
    "docs/templates/SLICE_PLAN.md", "docs/templates/EVIDENCE.md",
    "docs/templates/HANDOFF.md", ".github/CODEOWNERS",
    ".github/PULL_REQUEST_TEMPLATE.md", ".github/workflows/plan-quality.yml",
    ".github/dependabot.yml", ".github/ISSUE_TEMPLATE/config.yml",
)
STATES = {"proposed", "ready", "in_progress", "in_review", "done", "blocked"}
LANES = {"cpu", "gpu", "manual", "benchmark", "optional-provider"}


def prose(markdown: str) -> str:
    """Ignore fenced examples when checking Markdown links and headings."""
    result = []
    fence = None
    for line in markdown.splitlines():
        marker = re.match(r"^\s*(`{3,}|~{3,})", line)
        if marker:
            token = marker.group(1)[0]
            if fence is None:
                fence = token
            elif token == fence:
                fence = None
            continue
        if fence is None:
            result.append(line)
    return "\n".join(result)


def headings(markdown: str) -> set[str]:
    anchors: set[str] = set()
    counts: dict[str, int] = {}
    for line in prose(markdown).splitlines():
        match = re.match(r"^#{1,6}\s+(.+?)\s*#*\s*$", line)
        if not match:
            continue
        slug = re.sub(r"[^\w\- ]", "", match.group(1).lower()).replace(" ", "-")
        count = counts.get(slug, 0)
        counts[slug] = count + 1
        anchors.add(slug if count == 0 else f"{slug}-{count}")
    return anchors


def inside(root: Path, path: Path) -> bool:
    return path.resolve().is_relative_to(root.resolve())


def validate(root: Path = ROOT) -> list[str]:
    errors: list[str] = []
    root = root.resolve()
    for relative in REQUIRED:
        if not (root / relative).is_file():
            errors.append(f"Missing required file: {relative}")

    markdown = {
        path: path.read_text(encoding="utf-8-sig")
        for path in root.rglob("*.md")
        if not any(part in {".git", "build", "out", "artifacts", ".venv"}
                   for part in path.relative_to(root).parts)
    }
    for path, content in markdown.items():
        for match in re.finditer(r"\[[^\]\n]+\]\(([^)\n]+)\)", prose(content)):
            link = match.group(1).strip().strip("<>")
            parsed = urlsplit(link)
            if parsed.scheme or parsed.netloc:
                continue
            target = path if not parsed.path else path.parent / unquote(parsed.path)
            if not inside(root, target):
                errors.append(f"Link escapes repository in {path.relative_to(root)}: {link}")
                continue
            target = target.resolve()
            if not target.exists():
                errors.append(f"Broken link in {path.relative_to(root)}: {link}")
            elif parsed.fragment and target.suffix == ".md":
                text = markdown.get(target, target.read_text(encoding="utf-8-sig"))
                if unquote(parsed.fragment) not in headings(text):
                    errors.append(f"Missing anchor in {path.relative_to(root)}: {link}")

    try:
        backlog = json.loads((root / "planning/backlog.json").read_text(encoding="utf-8-sig"))
    except (OSError, ValueError) as exc:
        errors.append(f"Cannot read backlog: {exc}")
        return errors
    if not isinstance(backlog, dict) or backlog.get("schema_version") != 1:
        errors.append("Backlog must be an object with schema_version 1")
        return errors
    items = backlog.get("items")
    if not isinstance(items, list) or not items:
        errors.append("Backlog needs a non-empty items list")
        return errors

    by_id: dict[str, dict] = {}
    examples: set[str] = set()
    roadmap = markdown.get(root / "docs/ROADMAP.md", "")
    catalog = markdown.get(root / "examples/README.md", "")
    for item in items:
        if not isinstance(item, dict):
            errors.append("Backlog item must be an object")
            continue
        key = item.get("id", "")
        if not isinstance(key, str) or not re.fullmatch(r"PR-\d{3}(?:-[a-z0-9]+)?", key):
            errors.append(f"Invalid work-item ID: {key!r}")
            continue
        if key in by_id:
            errors.append(f"Duplicate work-item ID: {key}")
        by_id[key] = item
        for field in ("title", "milestone", "example", "example_spec", "acceptance",
                      "negative_case", "scope", "evidence", "rollback"):
            if not isinstance(item.get(field), str) or not item[field].strip():
                errors.append(f"{key}: missing non-empty {field}")
        state = item.get("status")
        if not isinstance(state, str) or state not in STATES:
            errors.append(f"{key}: invalid status {state!r}")
            state = None
        if backlog.get("phase") == "design" and state != "proposed":
            errors.append(f"{key}: design phase cannot claim execution state {state}")
        deps = item.get("depends_on")
        if not isinstance(deps, list) or not all(isinstance(dep, str) for dep in deps):
            errors.append(f"{key}: depends_on must be a list of IDs")
        elif len(set(deps)) != len(deps):
            errors.append(f"{key}: duplicate dependencies")
        lanes = item.get("lanes")
        if not isinstance(lanes, list) or not lanes or not all(
                isinstance(lane, str) and lane in LANES for lane in lanes):
            errors.append(f"{key}: invalid validation lanes")
        example = item.get("example")
        if isinstance(example, str):
            if example in examples:
                errors.append(f"Duplicate example ID: {example}")
            examples.add(example)
        relative = item.get("example_spec", "")
        if isinstance(relative, str) and relative:
            spec = root / relative
            if not inside(root / "examples", spec) or not spec.is_file():
                errors.append(f"{key}: missing or unsafe example spec {relative}")
            else:
                content = spec.read_text(encoding="utf-8-sig")
                for field in ("id", "example", "acceptance", "negative_case", "scope", "evidence", "rollback"):
                    value = item.get(field)
                    if isinstance(value, str) and value not in content:
                        errors.append(f"{key}: example spec differs from backlog field {field}")
                if state == "proposed" and "not implemented" not in content:
                    errors.append(f"{key}: proposed example lacks unimplemented status")
                if isinstance(deps, list) and all(isinstance(dep, str) for dep in deps):
                    expected_deps = ", ".join(deps) or "documentation bootstrap"
                    if f"Dependencies: {expected_deps}." not in content:
                        errors.append(f"{key}: example dependency declarations differ from backlog")
                if isinstance(lanes, list) and all(isinstance(lane, str) for lane in lanes):
                    if f"Validation lanes: {', '.join(lanes)}." not in content:
                        errors.append(f"{key}: example lane declarations differ from backlog")
            if relative not in roadmap or key not in roadmap:
                errors.append(f"{key}: missing roadmap link")
            if relative.removeprefix("examples/") not in catalog or key not in catalog:
                errors.append(f"{key}: missing example catalog link")
            if isinstance(deps, list) and all(isinstance(dep, str) for dep in deps):
                row = f"| {key} | {item.get('title')} | {', '.join(deps) or 'Bootstrap'} |"
                if row not in roadmap:
                    errors.append(f"{key}: roadmap title/dependencies differ from backlog")
            if isinstance(lanes, list) and all(isinstance(lane, str) for lane in lanes):
                row = f"| {key} | [{example}]({relative.removeprefix('examples/')}) | {', '.join(lanes)} |"
                if row not in catalog:
                    errors.append(f"{key}: catalog example/lanes differ from backlog")
        if state in {"ready", "in_progress", "in_review", "done"}:
            execution = item.get("execution", {})
            if not isinstance(execution, dict) or not execution.get("authorization"):
                errors.append(f"{key}: execution state needs adopted authorization reference")
            if state == "done" and (not isinstance(execution, dict) or not all(
                    execution.get(field) for field in ("pr_url", "merge_sha", "evidence"))):
                errors.append(f"{key}: done needs PR URL, merge SHA and evidence")

    expected_ids = set(by_id)
    for label, content in (("roadmap", roadmap), ("catalog", catalog)):
        declared_ids = set(re.findall(r"^\| (PR-\d{3}(?:-[a-z0-9]+)?) \|", content, re.MULTILINE))
        if declared_ids != expected_ids:
            errors.append(f"{label}: work-item rows differ from backlog IDs")
    registered_specs = {item.get("example_spec") for item in by_id.values()
                        if isinstance(item.get("example_spec"), str)}
    for spec in (root / "examples").glob("*.md"):
        if spec.name != "README.md" and spec.relative_to(root).as_posix() not in registered_specs:
            errors.append(f"Unregistered example specification: {spec.name}")

    visited: set[str] = set()
    active: set[str] = set()

    def visit(key: str) -> None:
        if key in active:
            errors.append(f"Dependency cycle involving {key}")
            return
        if key in visited:
            return
        active.add(key)
        item = by_id[key]
        deps = item.get("depends_on", [])
        for dependency in deps if isinstance(deps, list) else []:
            if not isinstance(dependency, str) or dependency not in by_id:
                errors.append(f"{key}: unknown dependency {dependency!r}")
                continue
            if isinstance(item.get("status"), str) and item["status"] in {"ready", "in_progress", "in_review", "done"}:
                if by_id[dependency].get("status") != "done":
                    errors.append(f"{key}: dependency {dependency} is not done")
            visit(dependency)
        active.remove(key)
        visited.add(key)

    for key in by_id:
        visit(key)
    return errors


def main() -> int:
    errors = validate()
    if errors:
        for error in errors:
            print(f"ERROR: {error}", file=sys.stderr)
        return 1
    data = json.loads((ROOT / "planning/backlog.json").read_text(encoding="utf-8-sig"))
    print(f"Planning validation passed: {len(data['items'])} work items with linked example specs; "
          "required files, local Markdown links and dependency graph checked.")
    print("Engine/runtime/GPU functionality: not implemented or tested by this validator.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
