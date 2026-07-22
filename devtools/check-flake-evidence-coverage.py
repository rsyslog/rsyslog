#!/usr/bin/env python3
"""Audit CI test workflows for the flake-evidence contract."""

from __future__ import annotations

import re
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
WORKFLOWS = ROOT / ".github/workflows"
HARVESTER = WORKFLOWS / "collect_flake_evidence.yml"
EXPECTED_UPLOADS = {
    "run_checks.yml": 7,
    "daily_focused_tests.yml": 1,
    "run_distro_daily.yml": 1,
    "run_cross_arch_weekly.yml": 1,
    "run_macos_weekly.yml": 1,
    "imbeats.yml": 1,
    "imhttp_prometheus_scrape.yml": 1,
    "impstats_push_victoriametrics.yml": 1,
}
TEST_COMMAND_RE = re.compile(r"run-ci\.sh|make\s+[^\n]*\b(?:check|distcheck)\b|devtools/test-[^\s]+\.sh")


def workflow_name(text: str) -> str | None:
    match = re.search(r"^name:\s*(.+?)\s*$", text, re.MULTILINE)
    return match.group(1) if match else None


def job_blocks(text: str):
    jobs = text.partition("\njobs:")[2]
    matches = list(re.finditer(r"^  ([A-Za-z0-9_-]+):\s*$", jobs, re.MULTILINE))
    for index, match in enumerate(matches):
        end = matches[index + 1].start() if index + 1 < len(matches) else len(jobs)
        yield match.group(1), jobs[match.start():end]


def main() -> int:
    errors = []
    harvester = HARVESTER.read_text(encoding="utf-8")
    for filename, minimum in EXPECTED_UPLOADS.items():
        path = WORKFLOWS / filename
        text = path.read_text(encoding="utf-8")
        count = text.count("uses: ./.github/actions/upload-flake-evidence")
        if count < minimum:
            errors.append(f"{filename}: expected at least {minimum} evidence uploads, found {count}")
        name = workflow_name(text)
        if not name or f"      - {name}\n" not in harvester:
            errors.append(f"{filename}: workflow name is missing from collect_flake_evidence.yml")

    for path in sorted(WORKFLOWS.glob("*.yml")):
        if path == HARVESTER:
            continue
        text = path.read_text(encoding="utf-8")
        for job_name, job in job_blocks(text):
            if TEST_COMMAND_RE.search(job) and "uses: ./.github/actions/upload-flake-evidence" not in job:
                errors.append(f"{path.name}:{job_name}: likely test commands found without an evidence upload")

    if errors:
        for error in errors:
            print(f"flake-evidence coverage error: {error}", file=sys.stderr)
        return 1
    print("Flake-evidence workflow coverage is complete.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
