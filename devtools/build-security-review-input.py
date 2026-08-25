#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 Rainer Gerhards and Others
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Build the deterministic input package for a local PR security review."""

from __future__ import annotations

import argparse
import fnmatch
import hashlib
import json
import re
import subprocess
import sys
from pathlib import Path, PurePosixPath


DEFAULT_MODEL = "doc/security/project-threat-model.md"
DEFAULT_COMPONENT_MAP = "doc/security/threat-model-components.json"
DEFAULT_OUTPUT = ".codex/security-review/input.json"
MODEL_ID_RE = re.compile(r"^### ((?:TB|SI)-[A-Z0-9-]+)\b", re.MULTILINE)
MODEL_REVISION_RE = re.compile(r"^Model revision: ([1-9][0-9]*)\s*$", re.MULTILINE)
SOURCE_SUFFIXES = {
    ".c", ".cc", ".conf", ".cpp", ".h", ".hh", ".hpp", ".json", ".m4", ".py", ".sh", ".yaml", ".yml",
}
SOURCE_BASENAMES = {"configure.ac", "Makefile.am", "Dockerfile", "Containerfile"}
SOURCE_PREFIXES = (
    ".github/",
    "compat/",
    "contrib/",
    "devtools/",
    "grammar/",
    "m4/",
    "packaging/",
    "plugins/",
    "runtime/",
    "scripts/",
    "tests/",
    "tools/",
)
SECURITY_POLICY_PATHS = {
    "AGENTS.md",
    "SECURITY.md",
    "doc/ai/security_triage_rubric.md",
    DEFAULT_MODEL,
    DEFAULT_COMPONENT_MAP,
}
SECURITY_POLICY_PREFIXES = (".agent/skills/rsyslog-security-pr-review/",)
EXPANDED_FILE_LIMIT = 50
EXPANDED_LINE_LIMIT = 2000


class ReviewInputError(Exception):
    """Raised for an invalid repository, model, or routing map."""


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", default=".", help="repository root (default: current directory)")
    parser.add_argument("--base", help="base ref for committed branch changes")
    parser.add_argument("--head", default="HEAD", help="head commit (default: HEAD)")
    parser.add_argument("--model", default=DEFAULT_MODEL, help="repository-relative threat model path")
    parser.add_argument("--component-map", default=DEFAULT_COMPONENT_MAP, help="repository-relative routing map path")
    parser.add_argument(
        "--output", default=DEFAULT_OUTPUT, help="output JSON path, relative to repo, or '-' for stdout"
    )
    return parser.parse_args()


def git(repo: Path, args: list[str], *, check: bool = True, text: bool = True):
    result = subprocess.run(
        ["git", *args],
        cwd=repo,
        check=False,
        capture_output=True,
        text=text,
    )
    if check and result.returncode != 0:
        stderr = result.stderr.strip() if text else result.stderr.decode("utf-8", "replace").strip()
        raise ReviewInputError(f"git {' '.join(args)} failed: {stderr}")
    return result


def verify_repo(path: Path) -> Path:
    result = git(path, ["rev-parse", "--show-toplevel"])
    return Path(result.stdout.strip()).resolve()


def verify_commit(repo: Path, ref: str) -> str | None:
    result = git(repo, ["rev-parse", "--verify", f"{ref}^{{commit}}"], check=False)
    return result.stdout.strip() if result.returncode == 0 else None


def resolve_default_base(repo: Path) -> tuple[str, str]:
    configured = git(repo, ["config", "--get", "rsyslog.localValidationBase"], check=False)
    if configured.returncode == 0 and configured.stdout.strip():
        return configured.stdout.strip(), "git-config"

    reflog = git(repo, ["reflog", "--format=%H", "HEAD"], check=False)
    reflog_entries = [line for line in reflog.stdout.splitlines() if line]
    if reflog_entries and verify_commit(repo, reflog_entries[-1]):
        return reflog_entries[-1], "worktree-reflog"

    return "origin/main", "fallback"


def resolve_base(repo: Path, requested: str | None) -> tuple[str, str, str]:
    if requested:
        base_ref, source = requested, "command-line"
    else:
        base_ref, source = resolve_default_base(repo)
    base_commit = verify_commit(repo, base_ref)
    if not base_commit:
        raise ReviewInputError(f"base ref '{base_ref}' is not available; fetch it or pass --base")
    return base_ref, base_commit, source


def parse_name_status_z(payload: bytes) -> list[dict[str, str]]:
    fields = payload.split(b"\0")
    entries: list[dict[str, str]] = []
    index = 0
    while index < len(fields) and fields[index]:
        status = fields[index].decode("ascii", "replace")
        index += 1
        if status.startswith(("R", "C")):
            if index + 1 >= len(fields):
                raise ReviewInputError("truncated rename/copy entry from git")
            old_path = fields[index].decode("utf-8", "surrogateescape")
            path = fields[index + 1].decode("utf-8", "surrogateescape")
            index += 2
            entries.append({"status": status[0], "old_path": old_path, "path": path})
        else:
            if index >= len(fields):
                raise ReviewInputError("truncated name-status entry from git")
            path = fields[index].decode("utf-8", "surrogateescape")
            index += 1
            entries.append({"status": status[0], "path": path})
    return entries


def combine_entry(current: dict[str, str] | None, incoming: dict[str, str]) -> dict[str, str]:
    if current is None:
        return dict(incoming)
    result = dict(current)
    incoming_status = incoming["status"]
    if incoming_status == "D":
        result["status"] = "D"
    elif current["status"] == "A" and incoming_status == "M":
        result["status"] = "A"
    elif current["status"] == "R" and incoming_status == "M":
        result["status"] = "R"
    else:
        result.update(incoming)
    if "old_path" in current and "old_path" not in result:
        result["old_path"] = current["old_path"]
    return result


def collect_changes(repo: Path, merge_base: str, head: str) -> tuple[list[dict[str, str]], list[str]]:
    combined: dict[str, dict[str, str]] = {}
    committed = git(
        repo,
        ["diff", "--name-status", "-z", "--find-renames", "--diff-filter=ACMRD", merge_base, head],
        text=False,
    )
    worktree = git(
        repo,
        ["diff", "--name-status", "-z", "--find-renames", "--diff-filter=ACMRD", head],
        text=False,
    )
    for entry in parse_name_status_z(committed.stdout) + parse_name_status_z(worktree.stdout):
        combined[entry["path"]] = combine_entry(combined.get(entry["path"]), entry)

    untracked_result = git(repo, ["ls-files", "--others", "--exclude-standard", "-z"], text=False)
    untracked = sorted(
        field.decode("utf-8", "surrogateescape") for field in untracked_result.stdout.split(b"\0") if field
    )
    for path in untracked:
        combined[path] = combine_entry(combined.get(path), {"status": "A", "path": path})
    return [combined[path] for path in sorted(combined)], untracked


def collect_numstat(repo: Path, merge_base: str, untracked: list[str]) -> tuple[dict[str, dict[str, object]], int]:
    result = git(repo, ["diff", "--numstat", "-z", "--find-renames", merge_base, "--"], text=False)
    by_path: dict[str, dict[str, object]] = {}
    total = 0
    fields = result.stdout.split(b"\0")
    index = 0
    while index < len(fields) and fields[index]:
        parts = fields[index].split(b"\t", 2)
        index += 1
        if len(parts) != 3:
            raise ReviewInputError("malformed numstat entry from git")
        added, deleted, path = parts
        paths: list[bytes]
        if path:
            paths = [path]
        else:
            if index + 1 >= len(fields):
                raise ReviewInputError("truncated rename/copy numstat entry from git")
            paths = [fields[index], fields[index + 1]]
            index += 2
        binary = added == b"-" or deleted == b"-"
        changed = 0 if binary else int(added) + int(deleted)
        for changed_path in paths:
            decoded_path = changed_path.decode("utf-8", "surrogateescape")
            by_path[decoded_path] = {"changed_lines": changed, "binary": binary}
        total += changed
    for path in untracked:
        file_path = repo / path
        data = file_path.read_bytes()
        binary = b"\0" in data
        changed = 0 if binary else len(data.splitlines())
        by_path[path] = {"changed_lines": changed, "binary": binary}
        total += changed
    return by_path, total


def path_matches(path: str, pattern: str) -> bool:
    # fnmatch gives repository-root-relative matching here. PurePath.match would
    # make a root pattern such as AGENTS.md also match doc/ai/AGENTS.md.
    return fnmatch.fnmatchcase(path, pattern)


def load_model(path: Path) -> tuple[set[str], int]:
    if not path.is_file():
        raise ReviewInputError(f"threat model is missing: {path}")
    try:
        text = path.read_text(encoding="utf-8")
    except UnicodeDecodeError as exc:
        raise ReviewInputError(f"invalid threat model UTF-8: {exc}") from exc
    ids = set(MODEL_ID_RE.findall(text))
    if not ids:
        raise ReviewInputError(f"threat model contains no TB-/SI- identifiers: {path}")
    revision_match = MODEL_REVISION_RE.search(text)
    if not revision_match:
        raise ReviewInputError(f"threat model contains no valid Model revision: {path}")
    return ids, int(revision_match.group(1))


def validate_relative_path(value: str, field: str) -> None:
    pure = PurePosixPath(value)
    if pure.is_absolute() or ".." in pure.parts or not value:
        raise ReviewInputError(f"invalid repository-relative {field}: {value!r}")


def load_components(path: Path, repo: Path, model_ids: set[str], model_revision: int) -> dict[str, object]:
    if not path.is_file():
        raise ReviewInputError(f"component map is missing: {path}")
    try:
        payload = json.loads(path.read_text(encoding="utf-8"))
    except (json.JSONDecodeError, UnicodeDecodeError) as exc:
        raise ReviewInputError(f"invalid component map JSON: {exc}") from exc
    if not isinstance(payload, dict) or payload.get("schema_version") != 1:
        raise ReviewInputError("component map schema_version must be 1")
    if not isinstance(payload.get("model_revision"), int) or payload["model_revision"] < 1:
        raise ReviewInputError("component map model_revision must be a positive integer")
    if payload["model_revision"] != model_revision:
        raise ReviewInputError(
            f"component map model_revision {payload['model_revision']} does not match threat model {model_revision}"
        )
    components = payload.get("components")
    if not isinstance(components, list) or not components:
        raise ReviewInputError("component map must contain a non-empty components list")

    seen: set[str] = set()
    required_arrays = ("paths", "boundaries", "invariants", "evidence", "test_hints")
    for component in components:
        if not isinstance(component, dict):
            raise ReviewInputError("each component must be an object")
        component_id = component.get("id")
        if not isinstance(component_id, str) or not component_id:
            raise ReviewInputError("each component needs a non-empty id")
        if component_id in seen:
            raise ReviewInputError(f"duplicate component id: {component_id}")
        seen.add(component_id)
        for field in required_arrays:
            values = component.get(field)
            if not isinstance(values, list) or any(not isinstance(value, str) for value in values):
                raise ReviewInputError(f"component {component_id} field {field} must be a string list")
        if not component["paths"]:
            raise ReviewInputError(f"component {component_id} has no paths")
        for pattern in component["paths"]:
            validate_relative_path(pattern, f"path pattern for {component_id}")
        for reference in component["boundaries"] + component["invariants"]:
            if reference not in model_ids:
                raise ReviewInputError(f"component {component_id} references unknown model id {reference}")
        for evidence in component["evidence"]:
            validate_relative_path(evidence, f"evidence path for {component_id}")
            if not (repo / evidence).exists():
                raise ReviewInputError(f"component {component_id} evidence path does not exist: {evidence}")
        if not isinstance(component.get("requires_lead"), bool):
            raise ReviewInputError(f"component {component_id} requires_lead must be boolean")
    return payload


def match_components(path: str, components: list[dict[str, object]]) -> list[dict[str, object]]:
    return [component for component in components if any(path_matches(path, pattern) for pattern in component["paths"])]


def is_reviewable(path: str, components: list[dict[str, object]], policy_paths: set[str]) -> bool:
    if path in policy_paths or path.startswith(SECURITY_POLICY_PREFIXES):
        return True
    pure = PurePosixPath(path)
    if pure.name in SOURCE_BASENAMES or pure.suffix.lower() in SOURCE_SUFFIXES:
        return path.startswith(SOURCE_PREFIXES) or "/" not in path
    if path.startswith("doc/source/") and pure.suffix.lower() in {".conf", ".json", ".yaml", ".yml"}:
        return True
    return any(component["id"] == "security-review-policy" for component in components)


def is_executable(repo: Path, merge_base: str, path: str) -> bool:
    current = repo / path
    if current.is_file() and current.stat().st_mode & 0o111:
        return True
    baseline = git(repo, ["ls-tree", merge_base, "--", path], check=False)
    return any(line.startswith("100755 ") for line in baseline.stdout.splitlines())


def compute_digest(
    repo: Path, merge_base: str, head_commit: str, untracked: list[str], model: Path, component_map: Path
) -> str:
    digest = hashlib.sha256()
    for label, value in (("merge-base", merge_base), ("head", head_commit)):
        digest.update(label.encode("ascii") + b"\0" + value.encode("ascii") + b"\0")
    diff = git(repo, ["diff", "--binary", "--full-index", "--no-ext-diff", merge_base, "--"], text=False)
    digest.update(b"tracked-diff\0" + diff.stdout)
    for path in sorted(untracked):
        encoded = path.encode("utf-8", "surrogateescape")
        digest.update(b"untracked\0" + encoded + b"\0" + (repo / path).read_bytes())
    digest.update(b"threat-model\0" + model.read_bytes())
    digest.update(b"component-map\0" + component_map.read_bytes())
    return digest.hexdigest()


def worktree_state(repo: Path, untracked: list[str]) -> dict[str, object]:
    staged = git(repo, ["diff", "--cached", "--quiet"], check=False).returncode != 0
    unstaged = git(repo, ["diff", "--quiet"], check=False).returncode != 0
    return {"staged": staged, "unstaged": unstaged, "untracked": bool(untracked)}


def build_package(args: argparse.Namespace) -> dict[str, object]:
    repo = verify_repo(Path(args.repo).resolve())
    head_commit = verify_commit(repo, args.head)
    if not head_commit:
        raise ReviewInputError(f"head ref '{args.head}' is not a commit")
    base_ref, base_commit, base_source = resolve_base(repo, args.base)
    merge_base = git(repo, ["merge-base", base_commit, head_commit]).stdout.strip()

    model_rel = PurePosixPath(args.model).as_posix()
    map_rel = PurePosixPath(args.component_map).as_posix()
    validate_relative_path(model_rel, "threat model path")
    validate_relative_path(map_rel, "component map path")
    model_path = repo / model_rel
    map_path = repo / map_rel
    model_ids, model_revision = load_model(model_path)
    routing = load_components(map_path, repo, model_ids, model_revision)
    components = routing["components"]
    policy_paths = SECURITY_POLICY_PATHS | {model_rel, map_rel}

    changes, untracked = collect_changes(repo, merge_base, head_commit)
    numstat, total_changed_lines = collect_numstat(repo, merge_base, untracked)
    reviewable_files: list[str] = []
    unmapped: list[str] = []
    lead_reasons: set[str] = set()
    enriched: list[dict[str, object]] = []

    for change in changes:
        path = change["path"]
        relevant_paths = [path]
        if "old_path" in change:
            relevant_paths.append(change["old_path"])
        matched = []
        seen_components: set[str] = set()
        for relevant_path in relevant_paths:
            for component in match_components(relevant_path, components):
                if component["id"] not in seen_components:
                    matched.append(component)
                    seen_components.add(component["id"])
        reviewable = any(
            is_reviewable(relevant_path, match_components(relevant_path, components), policy_paths)
            or is_executable(repo, merge_base, relevant_path)
            for relevant_path in relevant_paths
        )
        mapped_ids = [component["id"] for component in matched]
        boundaries = sorted({item for component in matched for item in component["boundaries"]})
        invariants = sorted({item for component in matched for item in component["invariants"]})
        if reviewable:
            reviewable_files.append(path)
            if not matched:
                mapped_ids = ["unmapped-code"]
                unmapped.append(path)
                lead_reasons.add(f"unmapped security-relevant path: {path}")
            for component in matched:
                if component["requires_lead"]:
                    lead_reasons.add(f"component requires lead review: {component['id']}")
        stats = numstat.get(path, {"changed_lines": 0, "binary": False})
        item: dict[str, object] = {
            **change,
            "reviewable": reviewable,
            "changed_lines": stats["changed_lines"],
            "binary": stats["binary"],
            "components": mapped_ids,
            "boundaries": boundaries,
            "invariants": invariants,
        }
        if change["status"] == "D" or "old_path" in change:
            item["baseline_location"] = f"{merge_base}:{change.get('old_path', path)}"
        enriched.append(item)

    reviewable_line_count = sum(
        int(item["changed_lines"]) for item in enriched if item["reviewable"]
    )
    if not reviewable_files:
        route = "not_applicable"
        reasons = ["no executable, source, build, workflow, sample-configuration, or security-policy changes"]
    elif len(reviewable_files) > EXPANDED_FILE_LIMIT or reviewable_line_count > EXPANDED_LINE_LIMIT:
        route = "expanded"
        reasons = [
            f"reviewable file/line budget exceeded: {len(reviewable_files)} files, {reviewable_line_count} lines"
        ]
        reasons.extend(sorted(lead_reasons))
    elif lead_reasons:
        route = "lead_required"
        reasons = sorted(lead_reasons)
    else:
        route = "quick"
        reasons = ["bounded mapped code delta"]

    package = {
        "schema_version": 1,
        "repository": ".",
        "base": {"ref": base_ref, "commit": base_commit, "source": base_source},
        "merge_base": merge_base,
        "head": {"ref": args.head, "commit": head_commit},
        "worktree": worktree_state(repo, untracked),
        "diff_digest": compute_digest(repo, merge_base, head_commit, untracked, model_path, map_path),
        "threat_model": {"path": model_rel, "revision": model_revision},
        "component_map": {"path": map_rel, "schema_version": routing["schema_version"]},
        "limits": {"source_files": EXPANDED_FILE_LIMIT, "changed_source_lines": EXPANDED_LINE_LIMIT},
        "summary": {
            "changed_files": len(enriched),
            "reviewable_files": len(reviewable_files),
            "changed_lines": total_changed_lines,
            "reviewable_changed_lines": reviewable_line_count,
        },
        "route": route,
        "route_reasons": reasons,
        "changes": enriched,
        "coverage": {
            "expected_files": reviewable_files,
            "unmapped_files": unmapped,
            "components": sorted(
                {component for item in enriched if item["reviewable"] for component in item["components"]}
            ),
            "boundaries": sorted(
                {boundary for item in enriched if item["reviewable"] for boundary in item["boundaries"]}
            ),
            "invariants": sorted(
                {invariant for item in enriched if item["reviewable"] for invariant in item["invariants"]}
            ),
            "review_status": "pending",
        },
    }
    return package


def write_package(package: dict[str, object], output: str, repo: Path) -> None:
    rendered = json.dumps(package, indent=2, sort_keys=False) + "\n"
    if output == "-":
        sys.stdout.write(rendered)
        return
    path = Path(output)
    if not path.is_absolute():
        path = repo / path
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(rendered, encoding="utf-8")
    print(path)


def main() -> int:
    args = parse_args()
    try:
        repo = verify_repo(Path(args.repo).resolve())
        package = build_package(args)
        write_package(package, args.output, repo)
    except (OSError, ReviewInputError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
