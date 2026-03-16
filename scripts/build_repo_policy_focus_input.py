#!/usr/bin/env python3
"""Build a focused repo-policy review package from a pull-request diff."""

from __future__ import annotations

import argparse
import json
import os
import re
import subprocess
from pathlib import Path


ROOT_DIR = Path(__file__).resolve().parents[1]
TOP_LEVEL_AGENTS = ROOT_DIR / "AGENTS.md"
DOC_AGENTS = ROOT_DIR / "doc" / "AGENTS.md"
TESTS_AGENTS = ROOT_DIR / "tests" / "AGENTS.md"
PLUGINS_AGENTS = ROOT_DIR / "plugins" / "AGENTS.md"
CONTRIB_AGENTS = ROOT_DIR / "contrib" / "AGENTS.md"
PARAMETER_RE = re.compile(r'\{\s*"([^"]+)"\s*,\s*eCmdHdlr')


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--base", help="base ref or sha")
    parser.add_argument("--head", help="head ref or sha")
    parser.add_argument("--output", required=True, help="output JSON path")
    return parser.parse_args()


def resolve_ref(cli_value: str | None, env_names: tuple[str, ...], default: str | None) -> str:
    if cli_value:
        return cli_value
    for env_name in env_names:
        value = os.environ.get(env_name)
        if value:
            return value
    if default is None:
        raise SystemExit(f"missing required ref; checked {', '.join(env_names)}")
    return default


def run_git(args: list[str], check: bool = True) -> str:
    result = subprocess.run(
        ["git", *args],
        cwd=ROOT_DIR,
        check=check,
        capture_output=True,
        text=True,
    )
    return result.stdout


def file_exists_at_ref(ref: str, path: str) -> bool:
    result = subprocess.run(
        ["git", "cat-file", "-e", f"{ref}:{path}"],
        cwd=ROOT_DIR,
        capture_output=True,
        text=True,
    )
    return result.returncode == 0


def read_file_at_ref(ref: str, path: str) -> str:
    return run_git(["show", f"{ref}:{path}"])


def collect_name_status(base: str, head: str) -> list[dict[str, object]]:
    entries = []
    output = run_git(["diff", "--name-status", "--find-renames", base, head])
    for line in output.splitlines():
        if not line.strip():
            continue
        parts = line.split("\t")
        status = parts[0]
        entry: dict[str, object] = {"status": status}
        if status.startswith("R") and len(parts) >= 3:
            entry["old_path"] = parts[1]
            entry["path"] = parts[2]
        elif len(parts) >= 2:
            entry["path"] = parts[1]
        else:
            continue
        entries.append(entry)
    return entries


def limited_diff(base: str, head: str, paths: list[str]) -> str:
    unique_paths = []
    for path in paths:
        if path and path not in unique_paths:
            unique_paths.append(path)
    if not unique_paths:
        return ""
    return run_git(["diff", "--unified=12", base, head, "--", *unique_paths])


def manifest_contains_subdir(manifest: str, module_dir: str) -> bool:
    pattern = re.compile(rf"(?<![A-Za-z0-9_./-]){re.escape(module_dir)}(?![A-Za-z0-9_./-])")
    return bool(pattern.search(manifest))


def configure_lists_makefile(configure_text: str, module_dir: str) -> bool:
    needle = f"{module_dir}/Makefile"
    pattern = re.compile(rf"(?<![A-Za-z0-9_./-]){re.escape(needle)}(?![A-Za-z0-9_./-])")
    return bool(pattern.search(configure_text))


def slugify_parameter_name(name: str) -> str:
    return re.sub(r"[^a-z0-9_-]+", "-", name.lower().replace(".", "-")).strip("-")


def extract_parameter_names(content: str) -> set[str]:
    return {match.group(1) for match in PARAMETER_RE.finditer(content)}


def build_tests_check(name_status: list[dict[str, object]], base: str, head: str) -> dict[str, object]:
    # This rule is strict: new or renamed shell tests must stay wired into
    # tests/Makefile.am, and lightweight -vg.sh wrappers should source the base
    # scenario instead of cloning its logic.
    added_tests: list[str] = []
    renamed_tests: list[dict[str, str]] = []
    for entry in name_status:
        path = str(entry.get("path", ""))
        if not path.startswith("tests/") or not path.endswith(".sh"):
            continue
        if Path(path).name in {"diag.sh"}:
            continue
        status = str(entry["status"])
        if status.startswith("A"):
            added_tests.append(path)
        elif status.startswith("R"):
            renamed_tests.append(
                {"old_path": str(entry["old_path"]), "path": path}
            )

    tests_makefile = (ROOT_DIR / "tests" / "Makefile.am").read_text(encoding="utf-8")
    tracked_tests = added_tests + [item["path"] for item in renamed_tests]
    missing_entries = [
        path for path in tracked_tests if Path(path).name not in tests_makefile
    ]
    stale_entries = [
        str(item["old_path"])
        for item in renamed_tests
        if Path(str(item["old_path"])).name in tests_makefile
    ]

    vg_wrappers = []
    for path in tracked_tests:
        if not path.endswith("-vg.sh"):
            continue
        content = read_file_at_ref(head, path)
        base_script = Path(path).name.replace("-vg.sh", ".sh")
        sources_base = base_script in content and ". " in content
        vg_wrappers.append(
            {
                "file": path,
                "base_script": f"tests/{base_script}",
                "sources_base": sources_base,
                "snippet": content[:600],
            }
        )

    applicable = bool(tracked_tests)
    relevant_paths = ["tests/Makefile.am", *tracked_tests]
    return {
        "id": "tests-registration",
        "title": "Test registration and wrapper policy",
        "applicable": applicable,
        "facts": {
            "added_tests": added_tests,
            "renamed_tests": renamed_tests,
            "missing_makefile_entries": missing_entries,
            "stale_makefile_entries": stale_entries,
            "vg_wrappers": vg_wrappers,
            "tests_makefile_touched": any(
                str(entry.get("path", "")) == "tests/Makefile.am" for entry in name_status
            ),
            "relevant_diff": limited_diff(base, head, relevant_paths) if applicable else "",
        },
    }


def build_doc_check(name_status: list[dict[str, object]], base: str, head: str) -> dict[str, object]:
    # doc/Makefile.am is the distribution source of truth for rst files under
    # doc/source/, so renamed or deleted docs can leave stale entries behind.
    changed_docs = []
    for entry in name_status:
        path = str(entry.get("path", ""))
        if path.startswith("doc/source/") and path.endswith(".rst"):
            changed_docs.append(
                {
                    "status": str(entry["status"]),
                    "path": path,
                    "old_path": str(entry.get("old_path", "")),
                }
            )

    doc_makefile = (ROOT_DIR / "doc" / "Makefile.am").read_text(encoding="utf-8")
    missing_entries = []
    stale_entries = []
    relevant_paths = ["doc/Makefile.am"]
    for item in changed_docs:
        path = item["path"]
        relative = path[len("doc/") :]
        if item["status"].startswith(("A", "R")) and relative not in doc_makefile:
            missing_entries.append(relative)
        if item["status"].startswith("D") and relative in doc_makefile:
            stale_entries.append(relative)
        if item["status"].startswith("R"):
            old_path = item["old_path"]
            if old_path:
                old_relative = old_path[len("doc/") :]
                if old_relative in doc_makefile:
                    stale_entries.append(old_relative)
        relevant_paths.append(path)

    applicable = bool(changed_docs)
    return {
        "id": "doc-dist-sync",
        "title": "Documentation distribution sync",
        "applicable": applicable,
        "facts": {
            "changed_docs": changed_docs,
            "missing_extra_dist_entries": sorted(set(missing_entries)),
            "stale_extra_dist_entries": sorted(set(stale_entries)),
            "doc_makefile_touched": any(
                str(entry.get("path", "")) == "doc/Makefile.am" for entry in name_status
            ),
            "relevant_diff": limited_diff(base, head, relevant_paths) if applicable else "",
        },
    }


def list_tree(ref: str, path: str) -> list[str]:
    output = run_git(["ls-tree", "-r", "--name-only", ref, "--", path], check=False)
    return [line for line in output.splitlines() if line]


def build_module_check(name_status: list[dict[str, object]], base: str, head: str) -> dict[str, object]:
    # Treat a brand-new plugins/ or contrib/ subtree with code or build files as
    # a module candidate, then check only for deterministic onboarding signals.
    candidate_dirs: dict[str, dict[str, object]] = {}
    for entry in name_status:
        path = str(entry.get("path", ""))
        parts = Path(path).parts
        if len(parts) < 2:
            continue
        if parts[0] not in {"plugins", "contrib"}:
            continue
        if parts[0] == "plugins" and parts[1] == "external":
            continue
        module_dir = f"{parts[0]}/{parts[1]}"
        candidate_dirs.setdefault(module_dir, {"tree": parts[0], "name": parts[1], "paths": []})
        candidate_dirs[module_dir]["paths"].append(path)

    new_modules = []
    for module_dir, info in sorted(candidate_dirs.items()):
        if file_exists_at_ref(base, module_dir):
            continue
        head_files = list_tree(head, module_dir)
        if not head_files:
            continue
        if not any(
            file.endswith(("Makefile.am", ".c", ".h", "MODULE_METADATA.yaml"))
            for file in head_files
        ):
            continue
        name = str(info["name"])
        metadata_path = f"{module_dir}/MODULE_METADATA.yaml"
        # The doc probe is heuristic on purpose: a likely module doc path is
        # enough for a warning/pass decision, but missing metadata is a hard
        # failure regardless of docs.
        doc_candidates = [
            f"doc/source/configuration/modules/{name}.rst",
            f"doc/source/configuration/modules/im{name}.rst",
            f"doc/source/configuration/modules/om{name}.rst",
        ]
        existing_docs = [path for path in doc_candidates if file_exists_at_ref(head, path)]
        new_modules.append(
            {
                "dir": module_dir,
                "tree": info["tree"],
                "has_metadata": file_exists_at_ref(head, metadata_path),
                "metadata_path": metadata_path,
                "doc_candidates": doc_candidates,
                "existing_docs": existing_docs,
                "changed_paths": info["paths"],
            }
        )

    applicable = bool(new_modules)
    relevant_paths = [item["dir"] for item in new_modules]
    return {
        "id": "module-onboarding",
        "title": "Module metadata and doc touchpoints",
        "applicable": applicable,
        "facts": {
            "new_modules": new_modules,
            "relevant_diff": limited_diff(base, head, relevant_paths) if applicable else "",
        },
    }


def build_module_build_wiring_check(module_check: dict[str, object], base: str, head: str) -> dict[str, object]:
    # New modules must be wired into the top-level automake/autoconf files or
    # they will never participate in normal builds and dist checks.
    new_modules = module_check["facts"]["new_modules"]
    applicable = bool(new_modules)
    top_makefile = read_file_at_ref(head, "Makefile.am") if applicable else ""
    configure_ac = read_file_at_ref(head, "configure.ac") if applicable else ""
    wiring = []
    for module in new_modules:
        module_dir = str(module["dir"])
        wiring.append(
            {
                "dir": module_dir,
                "makefile_listed": manifest_contains_subdir(top_makefile, module_dir),
                "configure_listed": configure_lists_makefile(configure_ac, module_dir),
            }
        )

    relevant_paths = ["Makefile.am", "configure.ac", *[item["dir"] for item in new_modules]]
    return {
        "id": "module-build-wiring",
        "title": "Module build wiring",
        "applicable": applicable,
        "facts": {
            "new_modules": wiring,
            "relevant_diff": limited_diff(base, head, relevant_paths) if applicable else "",
        },
    }


def build_parameter_doc_check(
    name_status: list[dict[str, object]], base: str, head: str
) -> dict[str, object]:
    # New module parameters should come with matching parameter reference docs.
    changed_sources = []
    parameter_docs = []
    for entry in name_status:
        path = str(entry.get("path", ""))
        base_path = str(entry.get("old_path", path))
        parts = Path(path).parts
        if len(parts) < 3:
            continue
        if parts[0] not in {"plugins", "contrib"}:
            continue
        if not path.endswith((".c", ".h")):
            continue
        if not file_exists_at_ref(head, path):
            continue

        head_params = extract_parameter_names(read_file_at_ref(head, path))
        base_params = set()
        if file_exists_at_ref(base, base_path):
            base_params = extract_parameter_names(read_file_at_ref(base, base_path))
        new_params = sorted(head_params - base_params)
        if not new_params:
            continue

        changed_sources.append(path)
        module_name = parts[1]
        for param in new_params:
            doc_path = (
                f"doc/source/reference/parameters/{module_name}-{slugify_parameter_name(param)}.rst"
            )
            parameter_docs.append(
                {
                    "module": module_name,
                    "source_file": path,
                    "parameter": param,
                    "doc_path": doc_path,
                    "has_doc": file_exists_at_ref(head, doc_path),
                }
            )

    applicable = bool(parameter_docs)
    relevant_paths = changed_sources + [
        item["doc_path"] for item in parameter_docs if item["has_doc"]
    ]
    return {
        "id": "parameter-doc-sync",
        "title": "Parameter documentation sync",
        "applicable": applicable,
        "facts": {
            "new_parameters": parameter_docs,
            "relevant_diff": limited_diff(base, head, relevant_paths) if applicable else "",
        },
    }


def collect_repo_guidance(checks: list[dict[str, object]]) -> dict[str, str]:
    guidance = {"AGENTS.md": TOP_LEVEL_AGENTS.read_text(encoding="utf-8")}
    is_applicable = {str(check["id"]): bool(check["applicable"]) for check in checks}
    if is_applicable.get("tests-registration"):
        guidance["tests/AGENTS.md"] = TESTS_AGENTS.read_text(encoding="utf-8")
    if is_applicable.get("doc-dist-sync") or is_applicable.get("parameter-doc-sync"):
        guidance["doc/AGENTS.md"] = DOC_AGENTS.read_text(encoding="utf-8")
    if (
        is_applicable.get("module-onboarding")
        or is_applicable.get("module-build-wiring")
        or is_applicable.get("parameter-doc-sync")
    ):
        guidance["plugins/AGENTS.md"] = PLUGINS_AGENTS.read_text(encoding="utf-8")
        guidance["contrib/AGENTS.md"] = CONTRIB_AGENTS.read_text(encoding="utf-8")
    return guidance


def main() -> int:
    args = parse_args()
    base = resolve_ref(args.base, ("AI_REVIEW_BASE", "GITHUB_BASE_SHA"), "HEAD")
    head = resolve_ref(args.head, ("AI_REVIEW_HEAD", "GITHUB_HEAD_SHA"), "HEAD")

    name_status = collect_name_status(base, head)
    changed_files = [str(entry.get("path", "")) for entry in name_status if entry.get("path")]
    module_check = build_module_check(name_status, base, head)
    checks = [
        build_tests_check(name_status, base, head),
        build_doc_check(name_status, base, head),
        module_check,
        build_module_build_wiring_check(module_check, base, head),
        build_parameter_doc_check(name_status, base, head),
    ]
    applicable_count = sum(1 for check in checks if check["applicable"])

    package = {
        "base": base,
        "head": head,
        "changed_files": changed_files,
        "applicable_count": applicable_count,
        "checks": checks,
        "repo_guidance": collect_repo_guidance(checks),
    }

    output_path = Path(args.output)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(json.dumps(package, indent=2) + "\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
