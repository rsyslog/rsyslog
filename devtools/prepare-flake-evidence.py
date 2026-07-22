#!/usr/bin/env python3
"""Build a versioned in-job flake evidence bundle after a failed test phase."""

from __future__ import annotations

import argparse
import json
import os
import re
import shutil
import sys
from pathlib import Path


AUTOMAKE_RE = re.compile(
    r"^(?:FAIL|ERROR): |^# (?:FAIL|ERROR):\s*[1-9][0-9]*\s*$|^FAIL: Test \./",
    re.MULTILINE,
)


def parse_phase(path: Path) -> dict:
    values = {}
    for line in path.read_text(encoding="utf-8").splitlines():
        key, separator, value = line.partition("=")
        if not separator:
            raise ValueError(f"invalid phase record: {path}")
        values[key] = value
    if values.get("type") not in ("automake", "custom", "distcheck"):
        raise ValueError(f"invalid phase type in {path}")
    if values.get("status") not in ("success", "failure", "interrupted"):
        raise ValueError(f"invalid phase status in {path}")
    exit_code = values.get("exit_code", "")
    return {
        "name": values.get("name", path.stem),
        "type": values["type"],
        "status": values["status"],
        "exit_code": int(exit_code) if exit_code else None,
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", default="flake-evidence", type=Path)
    args = parser.parse_args()
    evidence_root = Path(os.environ.get("RSYSLOG_FLAKE_EVIDENCE_ROOT", ".ci/flake-evidence"))
    phase_dir = evidence_root / "phases"
    if not phase_dir.is_dir():
        print("eligible=false")
        return 0
    phases = [parse_phase(path) for path in sorted(phase_dir.glob("*.phase"))]
    failed = [phase for phase in phases if phase["status"] in ("failure", "interrupted")]
    if not failed:
        print("eligible=false")
        return 0

    candidates = []
    candidates.extend(path for path in phase_dir.glob("*.phase") if path.is_file())
    standard_paths = [
        Path("failed-tests.log"), Path("tests/failed-tests.log"),
        Path("tests/test-suite.log"), Path("config.log"),
    ]
    for path in standard_paths:
        if path.is_file():
            candidates.append(path)
    for pattern in ("tests/*.log", "tests/*.trs", "core", "core.*", "tests/core", "tests/core.*"):
        candidates.extend(path for path in Path().glob(pattern) if path.is_file())
    if Path("/cores").is_dir():
        candidates.extend(path for path in Path("/cores").glob("core*") if path.is_file())
    if (evidence_root / "logs").is_dir():
        candidates.extend(path for path in (evidence_root / "logs").glob("*.log") if path.is_file())
    candidates = sorted(set(candidates))

    combined = "\n".join(
        path.read_text(encoding="utf-8", errors="replace")
        for path in candidates if path.suffix in (".log", ".trs", ".txt")
    )
    if all(phase["type"] == "distcheck" for phase in failed) and not AUTOMAKE_RE.search(combined):
        print("eligible=false")
        return 0

    if args.output.exists():
        shutil.rmtree(args.output)
    files_dir = args.output / "files"
    files_dir.mkdir(parents=True)
    included = []
    for index, source in enumerate(candidates):
        relative = Path("files") / f"{index:04d}-{source.name}"
        shutil.copy2(source, args.output / relative)
        included.append(relative.as_posix())

    source = {
        "repository": os.environ["GITHUB_REPOSITORY"],
        "workflow": os.environ["GITHUB_WORKFLOW"],
        "run_id": int(os.environ["GITHUB_RUN_ID"]),
        "run_attempt": int(os.environ.get("GITHUB_RUN_ATTEMPT", "1")),
        "job_id": os.environ["GITHUB_JOB"],
        "job_name": os.environ.get("RSYSLOG_FLAKE_JOB_NAME", os.environ["GITHUB_JOB"]),
        "head_sha": os.environ["GITHUB_SHA"],
        "event": os.environ["GITHUB_EVENT_NAME"],
        "url": (
            f"{os.environ['GITHUB_SERVER_URL']}/{os.environ['GITHUB_REPOSITORY']}"
            f"/actions/runs/{os.environ['GITHUB_RUN_ID']}"
        ),
    }
    manifest = {
        "schema_version": 1,
        "collector": "in-job",
        "entries": [{"source": source, "phases": phases, "files": included}],
    }
    (args.output / "flake-evidence.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    artifact_slug = re.sub(r"[^A-Za-z0-9._-]+", "-", source["job_name"]).strip("-")
    print("eligible=true")
    print(f"artifact_name=ci-failure-{source['run_id']}-{source['run_attempt']}-{artifact_slug}")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (KeyError, OSError, ValueError) as error:
        print(f"could not prepare flake evidence: {error}", file=sys.stderr)
        sys.exit(2)
