#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Select existing daily-stable package routes from the target inventory.

The inventory is the sole target catalog. This emits a dispatch plan only;
it neither runs workflows nor asserts package or production readiness.
"""

import argparse
import json
import re
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
DEFAULT_CATALOG = ROOT / ".github/packaging-targets.json"
ID_RE = re.compile(r"[a-z0-9.-]+\Z")
WORKFLOW_RE = re.compile(r"[a-z0-9_]+\.yml\Z")
POLICY_RE = re.compile(r"[a-z0-9_.-]+\.ya?ml\Z")
NATIVE_ARCH_RE = re.compile(r"[a-z0-9_]+\Z")
RUNNER_RE = re.compile(r"[a-z0-9.-]+\Z")
ARCHITECTURES = {"x86_64", "aarch64"}
FORMATS = {"deb", "rpm", "apk"}


def require(condition, message):
    if not condition:
        raise ValueError(message)


def repo_file(value, label):
    require(isinstance(value, str) and value, f"{label}: missing path")
    relative = Path(value)
    require(not relative.is_absolute() and all(
        part not in ("", ".", "..") for part in value.split("/")),
            f"{label}: unsafe path {value!r}")
    resolved = (ROOT / relative).resolve(strict=True)
    # Keep this helper usable by the older Python in legacy test lanes.
    require(ROOT in resolved.parents and resolved.is_file(),
            f"{label}: path escapes repository or is not a file: {value!r}")
    return relative


def load_catalog(path):
    catalog = json.loads(path.read_text(encoding="utf-8"))
    require(isinstance(catalog, dict) and type(catalog.get("schema_version")) is int and
            catalog["schema_version"] == 1,
            "catalog: unsupported schema_version")
    targets = catalog.get("targets")
    require(isinstance(targets, list), "catalog: targets must be a list")
    by_id = {}
    for row in targets:
        require(isinstance(row, dict), "catalog: target must be an object")
        target = row.get("id")
        require(isinstance(target, str) and ID_RE.fullmatch(target),
                f"catalog: invalid target id {target!r}")
        require(target not in by_id, f"catalog: duplicate target id {target}")
        require(isinstance(row.get("package_format"), str) and row["package_format"] in FORMATS,
                f"{target}: unsupported package format")
        arches = row.get("architectures")
        require(isinstance(arches, list) and arches and
                all(isinstance(arch, str) and arch in ARCHITECTURES for arch in arches) and
                len(arches) == len(set(arches)),
                f"{target}: invalid or duplicate canonical architecture")
        route = row.get("existing_archive")
        require(route is None or isinstance(route, dict),
                f"{target}: existing_archive must be an object or null")
        by_id[target] = row
    aliases = catalog.get("architecture_aliases", {})
    require(isinstance(aliases, dict), "catalog: architecture_aliases must be an object")
    return targets, by_id, aliases


def route_workflow(row):
    target = row["id"]
    route = row["existing_archive"]
    require(route is not None, f"{target}: no existing archive route")
    workflow = repo_file(route.get("workflow"), f"{target} workflow")
    require(workflow.parent == Path(".github/workflows") and
            WORKFLOW_RE.fullmatch(workflow.name),
            f"{target}: invalid workflow location or basename")
    repo_file(route.get("policy"), f"{target} policy")
    repo_file(route.get("builder"), f"{target} builder")
    return workflow.name


def build_plan(row, workflow, native_json, aliases):
    target = row["id"]
    require(route_workflow(row) == workflow, f"{target}: workflow does not match catalog route")
    policy = repo_file(row["existing_archive"].get("policy"), f"{target} policy")
    require(policy.parent == Path(".github") and POLICY_RE.fullmatch(policy.name),
            f"{target}: unsafe policy filename")
    format_aliases = aliases.get(row["package_format"], {})
    require(isinstance(format_aliases, dict), f"{target}: invalid architecture aliases")
    expected = []
    for arch in row["architectures"]:
        native = format_aliases.get(arch, arch)
        require(isinstance(native, str) and NATIVE_ARCH_RE.fullmatch(native),
                f"{target}: invalid native architecture alias")
        expected.append(native)
    require(len(expected) == len(set(expected)), f"{target}: duplicate native architecture alias")

    matrix = json.loads(native_json)
    require(isinstance(matrix, dict) and set(matrix) == {"include"} and
            isinstance(matrix["include"], list),
            "native matrix: expected only an include list")
    by_arch = {}
    for native_row in matrix["include"]:
        require(isinstance(native_row, dict), "native matrix: row must be an object")
        arch, runner = native_row.get("arch"), native_row.get("runner")
        require(isinstance(arch, str) and NATIVE_ARCH_RE.fullmatch(arch),
                "native matrix: invalid arch")
        require(isinstance(runner, str) and RUNNER_RE.fullmatch(runner),
                "native matrix: invalid runner")
        require(arch not in by_arch, f"native matrix: duplicate arch {arch}")
        by_arch[arch] = native_row
    require(all(arch in by_arch for arch in expected),
            f"{target}: requested architecture unavailable in native matrix")
    return {"matrix": {"include": [by_arch[arch] for arch in expected]}, "policy": str(policy)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--catalog", type=Path, default=DEFAULT_CATALOG)
    choice = parser.add_mutually_exclusive_group(required=True)
    choice.add_argument("--targets", help="Comma-separated target IDs, or 'existing'")
    choice.add_argument("--list", action="store_true", help="Report every catalog target and route disposition")
    choice.add_argument("--build-target", help="Catalog target ID for a workflow build matrix")
    parser.add_argument("--workflow", help="Expected existing workflow basename for --build-target")
    parser.add_argument("--native-matrix", help="Reviewed workflow's JSON include matrix for --build-target")
    args = parser.parse_args()
    rows, by_id, aliases = load_catalog(args.catalog)
    if args.build_target is not None:
        require(args.workflow is not None and args.native_matrix is not None,
                "--build-target requires --workflow and --native-matrix")
        require(WORKFLOW_RE.fullmatch(args.workflow), "build target: invalid workflow basename")
        require(args.build_target in by_id, f"build target: unknown target {args.build_target}")
        print(json.dumps(build_plan(by_id[args.build_target], args.workflow,
                                    args.native_matrix, aliases), separators=(",", ":")))
        return
    require(args.workflow is None and args.native_matrix is None,
            "--workflow and --native-matrix require --build-target")
    if args.list:
        print(json.dumps({"targets": [
            {"target": row["id"], "architectures": row["architectures"],
             "route": "available" if row["existing_archive"] is not None else "missing"}
            for row in rows]}, separators=(",", ":")))
        return

    if args.targets == "existing":
        selected = [row for row in rows if row["existing_archive"] is not None]
    else:
        ids = args.targets.split(",")
        require(all(ID_RE.fullmatch(target) for target in ids),
                "targets: invalid or empty target ID")
        require(len(ids) == len(set(ids)), "targets: duplicate target ID")
        require(all(target in by_id for target in ids),
                "targets: unknown target ID: " + ",".join(target for target in ids if target not in by_id))
        selected = [by_id[target] for target in ids]

    require(selected, "targets: no existing archive routes selected")
    workflows = set()
    include = []
    for row in selected:
        workflow = route_workflow(row)
        require(workflow not in workflows, f"{row['id']}: duplicate workflow basename {workflow}")
        workflows.add(workflow)
        include.append({"target": row["id"], "workflow": workflow,
                        "architectures": row["architectures"]})
    print(json.dumps({"include": include}, separators=(",", ":")))


if __name__ == "__main__":
    try:
        main()
    except (ValueError, OSError, json.JSONDecodeError) as error:
        print(f"package-candidate-plan: {error}", file=sys.stderr)
        sys.exit(1)
