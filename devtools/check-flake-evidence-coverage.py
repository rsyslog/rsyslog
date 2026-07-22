#!/usr/bin/env python3
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
UPLOAD_RE = re.compile(
    r"^\s+uses:\s+\./\.github/actions/upload-flake-evidence\s*(?:#.*)?$",
    re.MULTILINE,
)


def workflow_name(text: str) -> str | None:
    match = re.search(r"^name:\s*(.+?)\s*$", text, re.MULTILINE)
    return match.group(1) if match else None


def job_blocks(text: str):
    jobs = text.partition("\njobs:")[2]
    matches = list(re.finditer(r"^  ([A-Za-z0-9_-]+):\s*$", jobs, re.MULTILINE))
    for index, match in enumerate(matches):
        end = matches[index + 1].start() if index + 1 < len(matches) else len(jobs)
        yield match.group(1), jobs[match.start():end]


def workflow_paths():
    return sorted((*WORKFLOWS.glob("*.yml"), *WORKFLOWS.glob("*.yaml")))


def registered_workflow_names(text: str) -> set[str]:
    match = re.search(
        r"^    workflows:\s*$\n(?P<items>(?:^      - .+\n)+)", text, re.MULTILINE,
    )
    if not match:
        return set()
    return {
        line.removeprefix("      - ").strip()
        for line in match.group("items").splitlines()
    }


def main() -> int:
    errors = []
    harvester = HARVESTER.read_text(encoding="utf-8")
    registered = registered_workflow_names(harvester)
    if not registered:
        errors.append("collect_flake_evidence.yml: could not parse workflow_run registration")
    for filename, minimum in EXPECTED_UPLOADS.items():
        path = WORKFLOWS / filename
        text = path.read_text(encoding="utf-8")
        count = len(UPLOAD_RE.findall(text))
        if count < minimum:
            errors.append(f"{filename}: expected at least {minimum} evidence uploads, found {count}")
        name = workflow_name(text)
        if not name or name not in registered:
            errors.append(f"{filename}: workflow name is missing from collect_flake_evidence.yml")

    for path in workflow_paths():
        if path == HARVESTER:
            continue
        text = path.read_text(encoding="utf-8")
        likely_test_workflow = bool(TEST_COMMAND_RE.search(text) or UPLOAD_RE.search(text))
        name = workflow_name(text)
        if likely_test_workflow and (not name or name not in registered):
            errors.append(f"{path.name}: likely test workflow is missing from the harvester")
        for job_name, job in job_blocks(text):
            if TEST_COMMAND_RE.search(job) and not UPLOAD_RE.search(job):
                errors.append(f"{path.name}:{job_name}: likely test commands found without an evidence upload")

    if errors:
        for error in errors:
            print(f"flake-evidence coverage error: {error}", file=sys.stderr)
        return 1
    print("Flake-evidence workflow coverage is complete.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
