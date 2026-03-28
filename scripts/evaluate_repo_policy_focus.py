#!/usr/bin/env python3
"""Deterministically evaluate focused repo-policy checks."""

from __future__ import annotations

import argparse
import json
from pathlib import Path


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", required=True, help="focused review package JSON")
    parser.add_argument("--output", required=True, help="evaluation output JSON")
    return parser.parse_args()


def load_json(path: Path) -> object:
    return json.loads(path.read_text(encoding="utf-8"))


def build_issue(file_path: str, message: str, line: int = 0) -> dict[str, object]:
    return {"file": file_path, "line": line, "message": message}


def evaluate_tests_registration(check: dict[str, object]) -> dict[str, object]:
    # Registration issues are deterministic and should block immediately because
    # they break the test inventory rather than merely lowering confidence.
    facts = check["facts"]
    added_tests = facts["added_tests"]
    renamed_tests = facts["renamed_tests"]
    missing_entries = facts["missing_makefile_entries"]
    stale_entries = facts.get("stale_makefile_entries", [])
    vg_wrappers = facts["vg_wrappers"]
    issues: list[dict[str, object]] = []

    for path in missing_entries:
        issues.append(
            build_issue(
                "tests/Makefile.am",
                f"Missing entry for '{path}'.",
            )
        )
    for path in stale_entries:
        issues.append(
            build_issue(
                "tests/Makefile.am",
                f"Stale entry remains for renamed test '{path}'.",
            )
        )
    for wrapper in vg_wrappers:
        if not wrapper["sources_base"]:
            issues.append(
                build_issue(
                    wrapper["file"],
                    f"Wrapper should source '{wrapper['base_script']}' instead of duplicating logic.",
                )
            )

    if issues:
        return {
            "id": check["id"],
            "status": "fail",
            "confidence": "high",
            "reason": "Test registration or wrapper policy needs follow-up.",
            "issues": issues,
        }

    tracked = added_tests + [item["path"] for item in renamed_tests]
    if tracked:
        return {
            "id": check["id"],
            "status": "pass",
            "confidence": "high",
            "reason": "New or renamed tests are registered and wrappers follow the expected pattern.",
            "issues": [],
        }

    return {
        "id": check["id"],
        "status": "not_applicable",
        "confidence": "low",
        "reason": "Check not applicable.",
        "issues": [],
    }


def evaluate_doc_dist_sync(check: dict[str, object]) -> dict[str, object]:
    # EXTRA_DIST drift is a packaging bug, so this rule stays fully blocking.
    facts = check["facts"]
    missing_entries = facts["missing_extra_dist_entries"]
    stale_entries = facts["stale_extra_dist_entries"]
    issues = [
        build_issue("doc/Makefile.am", f"Missing EXTRA_DIST entry for '{path}'.")
        for path in missing_entries
    ]
    issues.extend(
        build_issue("doc/Makefile.am", f"Stale EXTRA_DIST entry remains for '{path}'.")
        for path in stale_entries
    )

    if issues:
        return {
            "id": check["id"],
            "status": "fail",
            "confidence": "high",
            "reason": "Documentation distribution metadata is out of sync with the doc tree.",
            "issues": issues,
        }

    if facts["changed_docs"]:
        return {
            "id": check["id"],
            "status": "pass",
            "confidence": "high",
            "reason": "Documentation distribution metadata matches the changed doc files.",
            "issues": [],
        }

    return {
        "id": check["id"],
        "status": "not_applicable",
        "confidence": "low",
        "reason": "Check not applicable.",
        "issues": [],
    }


def evaluate_doc_xref_sync(check: dict[str, object]) -> dict[str, object]:
    # Broken :doc: cross-references cause Sphinx build failures (CI treats
    # warnings as errors), so this rule is fully blocking.
    facts = check["facts"]
    broken_refs = facts["broken_refs"]
    issues = [
        build_issue(
            ref["file"],
            f":doc:`{ref['ref']}` resolves to '{ref['resolved']}' which is not registered in doc/Makefile.am.",
        )
        for ref in broken_refs
    ]

    if issues:
        return {
            "id": check["id"],
            "status": "fail",
            "confidence": "high",
            "reason": "One or more :doc: cross-references point to unregistered or missing RST files.",
            "issues": issues,
        }

    if facts["changed_docs"]:
        return {
            "id": check["id"],
            "status": "pass",
            "confidence": "high",
            "reason": "All :doc: cross-references in changed RST files resolve to registered targets.",
            "issues": [],
        }

    return {
        "id": check["id"],
        "status": "not_applicable",
        "confidence": "low",
        "reason": "Check not applicable.",
        "issues": [],
    }



    # Missing metadata is a hard policy failure; missing docs stay advisory
    # until rsyslog has a stricter deterministic doc coverage rule.
    facts = check["facts"]
    issues: list[dict[str, object]] = []
    warnings: list[dict[str, object]] = []

    for module in facts["new_modules"]:
        if not module["has_metadata"]:
            issues.append(
                build_issue(
                    module["dir"],
                    f"New module is missing '{module['metadata_path']}'.",
                )
            )
        if not module["existing_docs"]:
            warnings.append(
                build_issue(
                    module["dir"],
                    "No likely module documentation file was detected under doc/source/configuration/modules/.",
                )
            )

    if issues:
        return {
            "id": check["id"],
            "status": "fail",
            "confidence": "high",
            "reason": "New module onboarding is missing required metadata.",
            "issues": issues + warnings,
        }

    if warnings:
        return {
            "id": check["id"],
            "status": "warn",
            "confidence": "medium",
            "reason": "New module metadata exists, but documentation touchpoints were not detected.",
            "issues": warnings,
        }

    if facts["new_modules"]:
        return {
            "id": check["id"],
            "status": "pass",
            "confidence": "high",
            "reason": "New modules include metadata and a likely documentation touchpoint.",
            "issues": [],
        }

    return {
        "id": check["id"],
        "status": "not_applicable",
        "confidence": "low",
        "reason": "Check not applicable.",
        "issues": [],
    }


def evaluate_module_build_wiring(check: dict[str, object]) -> dict[str, object]:
    # Build wiring is deterministic: new modules should appear in the top-level
    # automake and autoconf manifests before they can be built in CI or distcheck.
    facts = check["facts"]
    issues: list[dict[str, object]] = []
    for module in facts["new_modules"]:
        if not module["makefile_listed"]:
            issues.append(
                build_issue(
                    "Makefile.am",
                    f"Missing SUBDIRS entry for '{module['dir']}'.",
                )
            )
        if not module["configure_listed"]:
            issues.append(
                build_issue(
                    "configure.ac",
                    f"Missing AC_CONFIG_FILES entry for '{module['dir']}/Makefile'.",
                )
            )

    if issues:
        return {
            "id": check["id"],
            "status": "fail",
            "confidence": "high",
            "reason": "New module build wiring is incomplete.",
            "issues": issues,
        }

    if facts["new_modules"]:
        return {
            "id": check["id"],
            "status": "pass",
            "confidence": "high",
            "reason": "New modules are wired into the top-level build manifests.",
            "issues": [],
        }

    return {
        "id": check["id"],
        "status": "not_applicable",
        "confidence": "low",
        "reason": "Check not applicable.",
        "issues": [],
    }


def evaluate_parameter_doc_sync(check: dict[str, object]) -> dict[str, object]:
    # This stays advisory for now because parameter extraction is deterministic,
    # but doc coverage still relies on a filename convention rather than parsing
    # the module reference pages end to end.
    facts = check["facts"]
    issues = [
        build_issue(
            item["source_file"],
            f"New parameter '{item['parameter']}' is missing '{item['doc_path']}'.",
        )
        for item in facts["new_parameters"]
        if not item["has_doc"]
    ]

    if issues:
        return {
            "id": check["id"],
            "status": "warn",
            "confidence": "medium",
            "reason": "New parameters were detected without matching parameter reference docs.",
            "issues": issues,
        }

    if facts["new_parameters"]:
        return {
            "id": check["id"],
            "status": "pass",
            "confidence": "high",
            "reason": "New parameters have matching parameter reference docs.",
            "issues": [],
        }

    return {
        "id": check["id"],
        "status": "not_applicable",
        "confidence": "low",
        "reason": "Check not applicable.",
        "issues": [],
    }


def evaluate_check(check: dict[str, object]) -> dict[str, object]:
    # The workflow is intentionally closed over a small fixed rule set so each
    # check can carry its own deterministic semantics.
    if not check["applicable"]:
        return {
            "id": check["id"],
            "status": "not_applicable",
            "confidence": "low",
            "reason": "Check not applicable.",
            "issues": [],
        }
    if check["id"] == "tests-registration":
        return evaluate_tests_registration(check)
    if check["id"] == "doc-dist-sync":
        return evaluate_doc_dist_sync(check)
    if check["id"] == "doc-xref-sync":
        return evaluate_doc_xref_sync(check)
    if check["id"] == "module-onboarding":
        return evaluate_module_onboarding(check)
    if check["id"] == "module-build-wiring":
        return evaluate_module_build_wiring(check)
    if check["id"] == "parameter-doc-sync":
        return evaluate_parameter_doc_sync(check)
    return {
        "id": check["id"],
        "status": "warn",
        "confidence": "low",
        "reason": "No deterministic evaluator was implemented for this check.",
        "issues": [],
    }


def main() -> int:
    args = parse_args()
    review_input = load_json(Path(args.input))
    if not isinstance(review_input, dict):
        raise SystemExit("review input must be a JSON object")

    checks = [evaluate_check(check) for check in review_input["checks"]]
    if review_input["applicable_count"] == 0:
        summary = "Focused repo-policy evaluation was not needed for this patch."
    else:
        fail_count = sum(1 for check in checks if check["status"] == "fail")
        warn_count = sum(1 for check in checks if check["status"] == "warn")
        if fail_count:
            summary = "Focused repo-policy evaluation found required follow-up."
        elif warn_count:
            summary = "Focused repo-policy evaluation found advisory follow-up."
        else:
            summary = "Focused repo-policy evaluation found no issues in the applicable checks."

    output = {"summary": summary, "checks": checks}
    output_path = Path(args.output)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(json.dumps(output, indent=2) + "\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
