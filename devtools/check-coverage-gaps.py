#!/usr/bin/env python3
"""Report uncovered changed production lines from lcov coverage data.

This is an advisory helper for local development.  It intentionally reports
coverage gaps as review input instead of enforcing a percentage threshold.
"""

from __future__ import annotations

import argparse
import fnmatch
import json
import os
import re
import subprocess
import urllib.error
import urllib.parse
import urllib.request
from dataclasses import dataclass, field
from pathlib import Path
from typing import Iterable


PRODUCTION_PREFIXES = (
    "runtime/",
    "plugins/",
    "contrib/",
    "grammar/",
    "tools/",
)
PRODUCTION_ROOT_SUFFIXES = (".c", ".h", ".y", ".l")
LOW_VALUE_RE = re.compile(
    r"\b("
    r"errmsg|LogError|LogMsg|LogError\d|DBGPRINTF|DBGPRINTF\d|dbgprintf|"
    r"perror|fprintf|printf|snprintf|strerror|goto finalize_it|goto done|"
    r"free\(|free\w*\(|ABORT_FINALIZE|FINALIZE|RETiRet|CHKmalloc|ENOMEM"
    r")\b"
)
LOW_VALUE_FUNCTION_RE = re.compile(r"(cleanup|finalize|error|errmsg|free)", re.IGNORECASE)


@dataclass
class FunctionCoverage:
    start: int
    name: str
    hits: int | None = None


@dataclass
class FileCoverage:
    lines: dict[int, int] = field(default_factory=dict)
    functions: list[FunctionCoverage] = field(default_factory=list)
    branches: dict[int, list[int | None]] = field(default_factory=dict)

    def function_for_line(self, line: int) -> str | None:
        current: FunctionCoverage | None = None
        for func in sorted(self.functions, key=lambda f: f.start):
            if func.start <= line:
                current = func
            else:
                break
        return current.name if current else None


@dataclass(frozen=True)
class ChangedLine:
    path: str
    line: int


@dataclass
class Gap:
    path: str
    start: int
    end: int
    function: str | None
    classification: str
    reason: str
    evidence: str
    lines: list[int]


def run_git(repo_root: Path, args: list[str]) -> str:
    proc = subprocess.run(
        ["git", *args],
        cwd=repo_root,
        check=True,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    return proc.stdout


def discover_repo_root(start: Path) -> Path:
    try:
        return Path(run_git(start, ["rev-parse", "--show-toplevel"]).strip())
    except subprocess.CalledProcessError as exc:
        raise SystemExit(f"error: not inside a git repository: {exc.stderr.strip()}") from exc


def normalize_path(path: str, repo_root: Path) -> str:
    path = path.strip()
    if path.startswith("/"):
        try:
            return str(Path(path).resolve().relative_to(repo_root.resolve()))
        except ValueError:
            pass
    if path.startswith("./"):
        path = path[2:]
    return path.replace("\\", "/")


def parse_lcov(path: Path, repo_root: Path) -> dict[str, FileCoverage]:
    coverage: dict[str, FileCoverage] = {}
    current_path: str | None = None
    current = FileCoverage()
    def finish_record() -> None:
        nonlocal current_path, current
        if current_path is not None:
            current.functions.sort(key=lambda f: f.start)
            coverage[current_path] = current
        current_path = None
        current = FileCoverage()

    with path.open(encoding="utf-8") as stream:
        for raw_line in stream:
            line = raw_line.rstrip("\n")
            if line == "end_of_record":
                finish_record()
                continue
            if line.startswith("SF:"):
                finish_record()
                current_path = normalize_path(line[3:], repo_root)
            elif line.startswith("DA:") and current_path is not None:
                fields = line[3:].split(",")
                if len(fields) >= 2:
                    current.lines[int(fields[0])] = int(fields[1])
            elif line.startswith("FN:") and current_path is not None:
                start_s, name = line[3:].split(",", 1)
                current.functions.append(FunctionCoverage(int(start_s), name))
            elif line.startswith("FNDA:") and current_path is not None:
                hits_s, name = line[5:].split(",", 1)
                for func in current.functions:
                    if func.name == name:
                        func.hits = int(hits_s)
                        break
            elif line.startswith("BRDA:") and current_path is not None:
                fields = line[5:].split(",")
                if len(fields) == 4:
                    branch_line = int(fields[0])
                    taken = None if fields[3] == "-" else int(fields[3])
                    current.branches.setdefault(branch_line, []).append(taken)
    finish_record()
    return coverage


def parse_codecov_report(path: Path, repo_root: Path) -> dict[str, dict[int, int]]:
    with path.open(encoding="utf-8") as stream:
        report = json.load(stream)
    result: dict[str, dict[int, int]] = {}
    for item in report.get("files", []):
        name = item.get("name")
        if not isinstance(name, str):
            continue
        raw_line_cover = item.get("line_coverage")
        if raw_line_cover is None:
            raw_line_cover = item.get("line_cover") or {}
        line_cover: dict[int, int] = {}
        if isinstance(raw_line_cover, dict):
            for key, value in raw_line_cover.items():
                try:
                    line_cover[int(key)] = int(value or 0)
                except (TypeError, ValueError):
                    continue
        elif isinstance(raw_line_cover, list):
            for idx, value in enumerate(raw_line_cover, start=1):
                if value is None:
                    continue
                if isinstance(value, (list, tuple)) and len(value) >= 2:
                    line_no, hits = value[0], value[1]
                    try:
                        line_cover[int(line_no)] = int(hits or 0)
                    except (TypeError, ValueError):
                        continue
                    continue
                try:
                    line_cover[idx] = int(value)
                except (TypeError, ValueError):
                    continue
        result[normalize_path(name, repo_root)] = line_cover
    return result


def fetch_codecov_report(
    repo: str, ref: str, ref_type: str, token_env: str, timeout: float
) -> dict:
    query_name = "sha" if ref_type == "sha" else "branch"
    query = urllib.parse.urlencode({query_name: ref})
    url = f"https://api.codecov.io/api/v2/github/{repo}/report/?{query}"
    headers = {"Accept": "application/json"}
    token = os.environ.get(token_env)
    if token:
        headers["Authorization"] = f"Bearer {token}"
    request = urllib.request.Request(url, headers=headers)
    try:
        with urllib.request.urlopen(request, timeout=timeout) as response:
            return json.loads(response.read().decode("utf-8"))
    except urllib.error.HTTPError as exc:
        raise SystemExit(f"error: Codecov API returned HTTP {exc.code} for {url}") from exc
    except urllib.error.URLError as exc:
        raise SystemExit(f"error: failed to fetch Codecov report from {url}: {exc}") from exc


def write_temp_codecov_report(report: dict, repo_root: Path) -> dict[str, dict[int, int]]:
    # Reuse the same parser so file-record behavior stays identical for fetched
    # and fixture reports.
    import tempfile

    with tempfile.NamedTemporaryFile("w", encoding="utf-8", delete=False) as tmp:
        json.dump(report, tmp)
        tmp_path = Path(tmp.name)
    try:
        return parse_codecov_report(tmp_path, repo_root)
    finally:
        tmp_path.unlink(missing_ok=True)


def parse_diff(diff_text: str) -> list[ChangedLine]:
    changed: list[ChangedLine] = []
    current_path: str | None = None
    new_line = 0

    for line in diff_text.splitlines():
        if line.startswith("+++ "):
            path = line[4:]
            if path == "/dev/null":
                current_path = None
            else:
                current_path = path[2:] if path.startswith("b/") else path
            continue
        if line.startswith("@@ "):
            match = re.search(r"\+(\d+)(?:,(\d+))?", line)
            if not match:
                continue
            new_line = int(match.group(1))
            continue
        if current_path is None:
            continue
        if line.startswith("+") and not line.startswith("+++"):
            changed.append(ChangedLine(current_path, new_line))
            new_line += 1
        elif line.startswith("-") and not line.startswith("---"):
            continue
        else:
            new_line += 1
    return changed


def parse_codecov_ignores(path: Path | None) -> list[str]:
    if path is None or not path.exists():
        return []
    ignores: list[str] = []
    in_ignore = False
    ignore_indent: int | None = None
    for raw_line in path.read_text(encoding="utf-8").splitlines():
        stripped = raw_line.strip()
        if not stripped or stripped.startswith("#"):
            continue
        indent = len(raw_line) - len(raw_line.lstrip())
        if stripped == "ignore:":
            in_ignore = True
            ignore_indent = indent
            continue
        if in_ignore and ignore_indent is not None and indent <= ignore_indent:
            break
        if in_ignore and stripped.startswith("- "):
            value = stripped[2:].strip().strip("'\"")
            if value:
                ignores.append(value)
    return ignores


def is_ignored(path: str, patterns: Iterable[str]) -> bool:
    for pattern in patterns:
        if fnmatch.fnmatch(path, pattern) or fnmatch.fnmatch(path, pattern.rstrip("/") + "/**"):
            return True
    return False


def is_production_path(path: str) -> bool:
    if path.startswith(PRODUCTION_PREFIXES):
        return path.endswith(PRODUCTION_ROOT_SUFFIXES)
    return "/" not in path and path.endswith(PRODUCTION_ROOT_SUFFIXES)


def read_source_line(repo_root: Path, path: str, line: int) -> str:
    try:
        lines = (repo_root / path).read_text(encoding="utf-8", errors="ignore").splitlines()
    except OSError:
        return ""
    return lines[line - 1].strip() if 1 <= line <= len(lines) else ""


def classify_line(
    repo_root: Path,
    changed: ChangedLine,
    coverage: dict[str, FileCoverage],
    codecov: dict[str, dict[int, int]] | None,
    ignore_patterns: list[str],
) -> tuple[str, str, str] | None:
    path = changed.path
    if not is_production_path(path):
        return None
    if is_ignored(path, ignore_patterns):
        return ("ignored", "path is ignored by .codecov.yml", "ignore")

    local_file = coverage.get(path)
    local_hit = local_file.lines.get(changed.line) if local_file else None
    if local_hit is not None and local_hit > 0:
        return None

    codecov_hit: int | None = None
    if codecov is not None:
        codecov_hit = codecov.get(path, {}).get(changed.line)
        if codecov_hit is not None and codecov_hit > 0:
            return (
                "covered-by-ci-only",
                "uncovered by local lcov data but covered in Codecov report",
                "local-lcov,codecov",
            )

    source = read_source_line(repo_root, path, changed.line)
    func = local_file.function_for_line(changed.line) if local_file else None
    if LOW_VALUE_RE.search(source) or (func is not None and LOW_VALUE_FUNCTION_RE.search(func)):
        return (
            "low-value-uncovered",
            "changed line looks like diagnostics, cleanup, or defensive error handling",
            "local-lcov" + (",codecov" if codecov is not None else ""),
        )

    if local_file is None:
        return (
            "needs-review",
            "changed production file has no local coverage record",
            "local-lcov" + (",codecov" if codecov is not None else ""),
        )
    return (
        "important-gap",
        "changed production line was not executed by local coverage data",
        "local-lcov" + (",codecov" if codecov is not None else ""),
    )


def group_gaps(
    repo_root: Path,
    changed_lines: list[ChangedLine],
    coverage: dict[str, FileCoverage],
    codecov: dict[str, dict[int, int]] | None,
    ignore_patterns: list[str],
) -> list[Gap]:
    gaps: list[Gap] = []
    current: Gap | None = None

    for changed in sorted(changed_lines, key=lambda item: (item.path, item.line)):
        classified = classify_line(repo_root, changed, coverage, codecov, ignore_patterns)
        if classified is None:
            continue
        classification, reason, evidence = classified
        func = coverage.get(changed.path, FileCoverage()).function_for_line(changed.line)
        can_extend = (
            current is not None
            and current.path == changed.path
            and current.classification == classification
            and current.reason == reason
            and current.evidence == evidence
            and current.function == func
            and changed.line == current.end + 1
        )
        if can_extend:
            current.end = changed.line
            current.lines.append(changed.line)
        else:
            current = Gap(
                path=changed.path,
                start=changed.line,
                end=changed.line,
                function=func,
                classification=classification,
                reason=reason,
                evidence=evidence,
                lines=[changed.line],
            )
            gaps.append(current)
    return gaps


def format_text(gaps: list[Gap]) -> str:
    if not gaps:
        return "No uncovered changed production lines found.\n"
    counts: dict[str, int] = {}
    for gap in gaps:
        counts[gap.classification] = counts.get(gap.classification, 0) + 1
    out = ["Coverage gap advisory report", ""]
    out.append(
        "Summary: "
        + ", ".join(f"{name}={count}" for name, count in sorted(counts.items()))
    )
    out.append("")
    for gap in gaps:
        location = f"{gap.path}:{gap.start}" if gap.start == gap.end else f"{gap.path}:{gap.start}-{gap.end}"
        out.append(f"- {gap.classification}: {location}")
        if gap.function:
            out.append(f"  function: {gap.function}")
        out.append(f"  evidence: {gap.evidence}")
        out.append(f"  reason: {gap.reason}")
    out.append("")
    return "\n".join(out)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Advisory coverage-gap checker for changed rsyslog code."
    )
    parser.add_argument("--coverage", default="coverage.info", help="lcov tracefile")
    parser.add_argument("--base", default="upstream/main", help="base ref for git diff")
    parser.add_argument("--diff-file", help="read unified diff from file instead of git")
    parser.add_argument("--repo-root", help="repository root")
    parser.add_argument("--format", choices=("text", "json"), default="text")
    parser.add_argument("--codecov-report", help="local Codecov report JSON fixture/file")
    parser.add_argument("--codecov-ref", help="fetch Codecov full report for this branch or SHA")
    parser.add_argument("--codecov-ref-type", choices=("branch", "sha"), default="branch")
    parser.add_argument("--codecov-repo", default="rsyslog/repos/rsyslog")
    parser.add_argument("--codecov-token-env", default="CODECOV_API_TOKEN")
    parser.add_argument("--codecov-timeout", type=float, default=20.0)
    args = parser.parse_args(argv)

    repo_root = Path(args.repo_root).resolve() if args.repo_root else discover_repo_root(Path.cwd())
    coverage_path = Path(args.coverage)
    if not coverage_path.is_absolute():
        coverage_path = repo_root / coverage_path
    if not coverage_path.exists():
        raise SystemExit(f"error: coverage file not found: {coverage_path}")

    coverage = parse_lcov(coverage_path, repo_root)
    if args.diff_file:
        diff_path = Path(args.diff_file)
        diff_text = diff_path.read_text(encoding="utf-8")
    else:
        diff_text = run_git(repo_root, ["diff", "--unified=0", f"{args.base}...HEAD"])
    changed_lines = parse_diff(diff_text)
    ignore_patterns = parse_codecov_ignores(repo_root / ".codecov.yml")

    codecov: dict[str, dict[int, int]] | None = None
    if args.codecov_report:
        codecov = parse_codecov_report(Path(args.codecov_report), repo_root)
    elif args.codecov_ref:
        codecov = write_temp_codecov_report(
            fetch_codecov_report(
                args.codecov_repo,
                args.codecov_ref,
                args.codecov_ref_type,
                args.codecov_token_env,
                args.codecov_timeout,
            ),
            repo_root,
        )

    gaps = group_gaps(repo_root, changed_lines, coverage, codecov, ignore_patterns)
    if args.format == "json":
        print(json.dumps([gap.__dict__ for gap in gaps], indent=2, sort_keys=True))
    else:
        print(format_text(gaps), end="")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
