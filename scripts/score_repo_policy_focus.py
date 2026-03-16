#!/usr/bin/env python3
"""Normalize focused repo-policy review output into markdown and JSON."""

from __future__ import annotations

import argparse
import json
from pathlib import Path


VALID_STATUS = {"pass", "warn", "fail", "not_applicable"}
VALID_CONFIDENCE = {"high", "medium", "low"}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--review-input", required=True, help="focused review package JSON")
    parser.add_argument("--model-output", help="model output JSON")
    parser.add_argument("--summary-md", required=True, help="markdown summary output path")
    parser.add_argument("--summary-json", required=True, help="JSON summary output path")
    return parser.parse_args()


def load_json(path: Path) -> object:
    return json.loads(path.read_text(encoding="utf-8"))


def normalize_issue(item: object) -> dict[str, object]:
    if not isinstance(item, dict):
        return {"file": "", "line": 0, "message": "Invalid issue entry."}
    line = item.get("line")
    if not isinstance(line, int):
        line = 0
    return {
        "file": str(item.get("file", "")).strip(),
        "line": line,
        "message": str(item.get("message", "")).strip(),
    }


def normalize_check(raw_check: object, expected_id: str, applicable: bool) -> dict[str, object]:
    if not isinstance(raw_check, dict):
        raw_check = {}
    status = str(raw_check.get("status", "not_applicable"))
    if status not in VALID_STATUS:
        status = "not_applicable"
    confidence = str(raw_check.get("confidence", "low"))
    if confidence not in VALID_CONFIDENCE:
        confidence = "low"
    issues_payload = raw_check.get("issues", [])
    if not isinstance(issues_payload, list):
        issues_payload = []
    reason = str(raw_check.get("reason", "")).strip()
    issues = [normalize_issue(item) for item in issues_payload]
    if not applicable:
        status = "not_applicable"
        reason = "Check not applicable."
        issues = []
    return {
        "id": expected_id,
        "status": status,
        "confidence": confidence,
        "reason": reason,
        "issues": issues,
    }


def normalize_model_output(review_input: dict[str, object], payload: object) -> dict[str, object]:
    if not isinstance(payload, dict):
        payload = {}
    summary = str(payload.get("summary", "No evaluator summary was provided.")).strip()
    checks_payload = payload.get("checks", [])
    if not isinstance(checks_payload, list):
        checks_payload = []
    checks_by_id = {
        str(item.get("id")): item for item in checks_payload if isinstance(item, dict)
    }
    normalized_checks = []
    for check in review_input["checks"]:
        check_id = str(check["id"])
        normalized_checks.append(
            normalize_check(
                checks_by_id.get(check_id, {}),
                check_id,
                bool(check["applicable"]),
            )
        )
    return {"summary": summary, "checks": normalized_checks}


def build_unavailable_summary(review_input: dict[str, object]) -> dict[str, object]:
    return {
        "status": "skipped",
        "evaluation_mode": str(review_input.get("evaluation_mode", "unavailable")),
        "applicable_count": review_input.get("applicable_count", 0),
        "changed_files": review_input.get("changed_files", []),
        "summary": "Focused repo-policy evaluation was not run.",
        "checks": [
            {
                "id": str(check["id"]),
                "status": "not_applicable" if not check["applicable"] else "warn",
                "confidence": "low",
                "reason": "Evaluator unavailable." if check["applicable"] else "Check not applicable.",
                "issues": [],
            }
            for check in review_input["checks"]
        ],
    }


def build_scored_summary(review_input: dict[str, object], model_output: dict[str, object]) -> dict[str, object]:
    failing = [check for check in model_output["checks"] if check["status"] == "fail"]
    warnings = [check for check in model_output["checks"] if check["status"] == "warn"]
    status = "reviewed"
    summary = model_output["summary"] or "Focused repo-policy review completed."
    if review_input.get("applicable_count", 0) == 0:
        status = "skipped"
        summary = "No focused repo-policy checks were applicable to this patch."
    elif not summary or summary in {"Patch review", "short overall assessment"}:
        summary = "Focused repo-policy review completed, but the evaluator summary was too generic."
    return {
        "status": status,
        "evaluation_mode": str(review_input.get("evaluation_mode", "unknown")),
        "applicable_count": review_input.get("applicable_count", 0),
        "changed_files": review_input.get("changed_files", []),
        "summary": summary,
        "checks": model_output["checks"],
        "totals": {
            "fail": len(failing),
            "warn": len(warnings),
            "pass": len([check for check in model_output["checks"] if check["status"] == "pass"]),
        },
    }


def render_markdown(summary: dict[str, object]) -> str:
    lines = [
        "# Repo Policy Focus Review",
        "",
        f"- Status: {summary['status']}",
        f"- Evaluator: {summary.get('evaluation_mode', 'unknown')}",
        f"- Applicable checks: {summary['applicable_count']}",
        f"- Changed files: {len(summary['changed_files'])}",
        "",
        summary["summary"],
        "",
        "## Checks",
        "",
        "| Check | Status | Confidence | Reason |",
        "| --- | --- | --- | --- |",
    ]
    for check in summary["checks"]:
        reason = check["reason"] or "-"
        lines.append(
            f"| `{check['id']}` | {check['status']} | {check['confidence']} | {reason} |"
        )
    lines.extend(["", "## Issues", ""])

    any_issues = False
    for check in summary["checks"]:
        if not check["issues"]:
            continue
        any_issues = True
        lines.append(f"### `{check['id']}`")
        lines.append("")
        for issue in check["issues"]:
            location = issue["file"] or "(no file)"
            if issue["line"]:
                location = f"{location}:{issue['line']}"
            lines.append(f"- {location} - {issue['message']}")
        lines.append("")
    if not any_issues:
        lines.append("- None.")
        lines.append("")
    return "\n".join(lines)


def main() -> int:
    args = parse_args()
    review_input = load_json(Path(args.review_input))
    if not isinstance(review_input, dict):
        raise SystemExit("review input must be a JSON object")

    if not args.model_output:
        summary = build_unavailable_summary(review_input)
    else:
        normalized_output = normalize_model_output(review_input, load_json(Path(args.model_output)))
        summary = build_scored_summary(review_input, normalized_output)

    summary_md = Path(args.summary_md)
    summary_json = Path(args.summary_json)
    summary_md.parent.mkdir(parents=True, exist_ok=True)
    summary_json.parent.mkdir(parents=True, exist_ok=True)
    summary_md.write_text(render_markdown(summary), encoding="utf-8")
    summary_json.write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
