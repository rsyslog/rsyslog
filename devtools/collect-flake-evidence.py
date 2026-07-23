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
"""Harvest eligible failed-job logs from a completed GitHub Actions run."""

from __future__ import annotations

import argparse
import io
import json
import os
import re
import sys
import urllib.error
import urllib.parse
import urllib.request
import zipfile
from pathlib import Path


BEGIN_RE = re.compile(r"RSYSLOG_FLAKE_PHASE_BEGIN name=([A-Za-z0-9._-]+) type=(automake|custom|distcheck)")
END_RE = re.compile(
    r"RSYSLOG_FLAKE_PHASE_END name=([A-Za-z0-9._-]+) type=(automake|custom|distcheck) "
    r"status=(success|failure) exit_code=([0-9]+)"
)
AUTOMAKE_RE = re.compile(
    r"^(?:FAIL|ERROR): |^# (?:FAIL|ERROR):\s*[1-9][0-9]*\s*$|^FAIL: Test \./",
    re.MULTILINE,
)


def redirect_host_is_allowed(host: str) -> bool:
    return (
        host.endswith(".blob.core.windows.net") or
        host.endswith(".githubusercontent.com") or
        host in ("github.com", "objects.githubusercontent.com")
    )


class GitHub:
    def __init__(self, repo: str, token: str):
        self.base = f"https://api.github.com/repos/{repo}"
        self.headers = {
            "Accept": "application/vnd.github+json",
            "Authorization": f"Bearer {token}",
            "X-GitHub-Api-Version": "2022-11-28",
            "User-Agent": "rsyslog-flake-evidence-harvester",
        }

    def get(self, path: str) -> bytes:
        url = self.base + path
        parsed = urllib.parse.urlsplit(url)
        if parsed.scheme != "https" or parsed.hostname != "api.github.com":
            raise ValueError("refusing non-GitHub API URL")
        request = urllib.request.Request(url, headers=self.headers)
        opener = urllib.request.build_opener(NoRedirect)
        try:
            with opener.open(request, timeout=60) as response:  # nosec B310 - URL constrained above
                return response.read()
        except urllib.error.HTTPError as error:
            if error.code not in (301, 302, 303, 307, 308):
                raise
            return self.download_redirect(error.headers["Location"])

    def download_redirect(self, url: str) -> bytes:
        parsed = urllib.parse.urlsplit(url)
        host = parsed.hostname or ""
        if parsed.scheme != "https" or not redirect_host_is_allowed(host):
            raise ValueError(f"refusing unexpected GitHub log host: {host}")
        request = urllib.request.Request(url, headers={"User-Agent": self.headers["User-Agent"]})
        with urllib.request.urlopen(request, timeout=60) as response:  # nosec B310 - allowlisted HTTPS host
            return response.read()

    def json(self, path: str) -> dict:
        return json.loads(self.get(path))


class NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, request, file_pointer, code, message, headers, new_url):
        return None


def phases_from_log(text: str, job_name: str) -> list[dict]:
    phases: dict[tuple[str, str], dict] = {}
    for line in text.splitlines():
        begin = BEGIN_RE.search(line)
        if begin:
            phases[(begin.group(1), begin.group(2))] = {
                "name": begin.group(1), "type": begin.group(2),
                "status": "interrupted", "exit_code": None,
            }
        end = END_RE.search(line)
        if end:
            phases[(end.group(1), end.group(2))] = {
                "name": end.group(1), "type": end.group(2),
                "status": end.group(3), "exit_code": int(end.group(4)),
            }
    eligible = [phase for phase in phases.values() if phase["status"] in ("failure", "interrupted")]
    if eligible:
        return list(phases.values())
    if phases:
        return []
    if AUTOMAKE_RE.search(text):
        slug = re.sub(r"[^a-z0-9._-]+", "-", job_name.lower()).strip("-") or "legacy-job"
        return [{"name": f"legacy-{slug}", "type": "automake", "status": "failure", "exit_code": None}]
    return []


def normalize_log(text: str) -> str:
    text = text.removeprefix("\ufeff")
    return re.sub(
        r"^[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9:.]+Z\s", "", text,
        flags=re.MULTILINE,
    )


def decode_job_logs(raw: bytes) -> str:
    """Decode the ZIP returned by the job-log API without extracting files."""
    if not zipfile.is_zipfile(io.BytesIO(raw)):
        return raw.decode("utf-8", errors="replace")
    parts = []
    with zipfile.ZipFile(io.BytesIO(raw)) as archive:
        for info in sorted(archive.infolist(), key=lambda item: item.filename):
            if info.is_dir():
                continue
            text = archive.read(info).decode("utf-8", errors="replace")
            parts.append(f"===== {info.filename} =====\n{text}")
    return "\n".join(parts)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo", required=True)
    parser.add_argument("--run-id", required=True, type=int)
    parser.add_argument("--attempt", required=True, type=int)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    github = GitHub(args.repo, os.environ["GITHUB_TOKEN"])
    run = github.json(f"/actions/runs/{args.run_id}")
    jobs = github.json(f"/actions/runs/{args.run_id}/attempts/{args.attempt}/jobs?per_page=100")["jobs"]
    entries = []
    for job in jobs:
        if job.get("conclusion") not in ("failure", "timed_out"):
            continue
        raw = github.get(f"/actions/jobs/{job['id']}/logs")
        text = normalize_log(decode_job_logs(raw))
        phases = phases_from_log(text, job["name"])
        if not phases:
            continue
        job_dir = args.output / "jobs" / str(job["id"])
        job_dir.mkdir(parents=True, exist_ok=True)
        log_path = job_dir / "job.log"
        log_path.write_text(text, encoding="utf-8")
        entries.append({
            "source": {
                "repository": args.repo,
                "workflow": run["name"],
                "run_id": args.run_id,
                "run_attempt": args.attempt,
                "job_id": job["id"],
                "job_name": job["name"],
                "head_sha": run["head_sha"],
                "event": run["event"],
                "url": run["html_url"],
            },
            "phases": phases,
            "files": [log_path.relative_to(args.output).as_posix()],
        })
    if not entries:
        print("No eligible test failures found.")
        return 0
    manifest = {"schema_version": 1, "collector": "fallback", "entries": entries}
    (args.output / "flake-evidence.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(f"Prepared {len(entries)} fallback evidence entries.")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (KeyError, OSError, ValueError, json.JSONDecodeError) as error:
        print(f"fallback evidence harvest failed: {error}", file=sys.stderr)
        sys.exit(2)
