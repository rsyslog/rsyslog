#!/usr/bin/env python3
"""Validate marked rsyslog documentation config samples."""

import argparse
import dataclasses
import os
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path


MARKER_RE = re.compile(r"^(?P<indent>\s*)\.\. rsyslog-doc-sample:\s*validate-config\s*$")
OPTION_RE = re.compile(r"^\s+:(?P<key>[a-z-]+):\s*(?P<value>.*)$")
CODE_BLOCK_RE = re.compile(r"^(?P<indent>\s*)\.\. code-block::\s*(?P<language>\S+)\s*$")
DEFAULT_RSYSLOGD_TIMEOUT = 30.0


@dataclasses.dataclass
class DocSample:
    source: Path
    line: int
    label: str
    code: str
    required_plugins: list[str]
    prepend: list[str]
    append: list[str]


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Validate RST code blocks marked with '.. rsyslog-doc-sample: validate-config'."
    )
    parser.add_argument("--source-dir", default="doc/source", help="Documentation source tree to scan.")
    parser.add_argument("--build-dir", default=".", help="Configured rsyslog build tree.")
    parser.add_argument("--rsyslogd", default=None, help="Path to rsyslogd; defaults to BUILD_DIR/tools/rsyslogd.")
    parser.add_argument("--work-dir", default=None, help="Directory for generated sample configs and logs.")
    parser.add_argument(
        "--timeout",
        default=DEFAULT_RSYSLOGD_TIMEOUT,
        type=float,
        help="Seconds before a single rsyslogd config check times out.",
    )
    return parser.parse_args()


def read_metadata(lines: list[str], start: int) -> tuple[dict[str, list[str]], int]:
    metadata: dict[str, list[str]] = {"require-plugin": [], "prepend": [], "append": []}
    i = start
    while i < len(lines):
        if lines[i].strip() == "":
            i += 1
            continue
        match = OPTION_RE.match(lines[i])
        if match is None:
            break
        key = match.group("key")
        if key not in metadata:
            raise ValueError(f"unknown rsyslog-doc-sample option :{key}:")
        metadata[key].append(match.group("value"))
        i += 1
    return metadata, i


def read_code_block(lines: list[str], start: int) -> tuple[str, int, int]:
    i = start
    while i < len(lines) and lines[i].strip() == "":
        i += 1
    if i >= len(lines):
        raise ValueError("marker is not followed by a code block")

    match = CODE_BLOCK_RE.match(lines[i])
    if match is None:
        raise ValueError("marker must be followed by '.. code-block:: rsyslog'")
    language = match.group("language")
    if language != "rsyslog":
        raise ValueError(f"marked sample uses unsupported code-block language '{language}'")

    block_indent = len(match.group("indent"))
    block_line = i + 1
    i += 1
    while i < len(lines) and (lines[i].strip() == "" or OPTION_RE.match(lines[i])):
        i += 1

    code_lines: list[str] = []
    content_indent = None
    while i < len(lines):
        line = lines[i]
        if line.strip() == "":
            code_lines.append("")
            i += 1
            continue
        leading = len(line) - len(line.lstrip(" "))
        if leading <= block_indent:
            break
        if content_indent is None:
            content_indent = leading
        if leading < content_indent:
            break
        code_lines.append(line[content_indent:])
        i += 1

    while code_lines and code_lines[-1] == "":
        code_lines.pop()
    if not code_lines:
        raise ValueError("marked code block is empty")
    return "\n".join(code_lines) + "\n", i, block_line


def sample_label(path: Path, line: int) -> str:
    stem = re.sub(r"[^A-Za-z0-9_.-]+", "-", str(path.with_suffix(""))).strip("-")
    return f"{stem}-{line}"


def discover_samples(source_dir: Path) -> list[DocSample]:
    samples: list[DocSample] = []
    for path in sorted(source_dir.rglob("*.rst")):
        relpath = path.relative_to(source_dir)
        lines = path.read_text(encoding="utf-8").splitlines()
        i = 0
        while i < len(lines):
            match = MARKER_RE.match(lines[i])
            if match is None:
                i += 1
                continue
            marker_line = i + 1
            try:
                metadata, code_start = read_metadata(lines, i + 1)
                code, i, block_line = read_code_block(lines, code_start)
            except ValueError as exc:
                raise ValueError(f"{relpath}:{marker_line}: {exc}") from exc
            samples.append(
                DocSample(
                    source=relpath,
                    line=block_line,
                    label=sample_label(relpath, block_line),
                    code=code,
                    required_plugins=metadata["require-plugin"],
                    prepend=metadata["prepend"],
                    append=metadata["append"],
                )
            )
    return samples


def timeout_output(exc: subprocess.TimeoutExpired) -> str:
    stdout = exc.stdout or ""
    stderr = exc.stderr or ""
    if isinstance(stdout, bytes):
        stdout = stdout.decode(errors="replace")
    if isinstance(stderr, bytes):
        stderr = stderr.decode(errors="replace")
    return stdout + stderr


def plugin_available(build_dir: Path, plugin: str, rsyslogd: Path, timeout: float) -> bool:
    candidates = [
        build_dir / "plugins" / plugin / ".libs" / f"{plugin}.so",
        build_dir / "plugins" / plugin / ".libs" / f"{plugin}.la",
        build_dir / "plugins" / plugin / f"{plugin}.so",
        build_dir / "contrib" / plugin / ".libs" / f"{plugin}.so",
        build_dir / "contrib" / plugin / ".libs" / f"{plugin}.la",
        build_dir / "contrib" / plugin / f"{plugin}.so",
    ]
    if any(candidate.exists() for candidate in candidates):
        return True

    with tempfile.TemporaryDirectory(prefix="rsyslog-doc-sample-plugin-") as tmp:
        cfg = Path(tmp) / f"{plugin}.conf"
        cfg.write_text(f'module(load="{plugin}")\n', encoding="utf-8")
        try:
            result = subprocess.run(
                rsyslogd_check_command(rsyslogd, build_dir, cfg),
                cwd=build_dir,
                text=True,
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
                timeout=timeout,
                check=False,
            )
        except subprocess.TimeoutExpired:
            return False
    return result.returncode == 0


def module_search_path(build_dir: Path) -> str:
    candidates = [
        build_dir / "runtime" / ".libs",
        build_dir / ".libs",
        *sorted((build_dir / "plugins").glob("*/*")),
        *sorted((build_dir / "contrib").glob("*/*")),
    ]
    return os.pathsep.join(str(path) for path in candidates if path.name == ".libs" and path.is_dir())


def rsyslogd_check_command(rsyslogd: Path, build_dir: Path, cfg: Path) -> list[str]:
    cmd = [str(rsyslogd), "-C", "-N1"]
    module_dir = module_search_path(build_dir)
    if module_dir:
        cmd.append(f"-M{module_dir}")
    cmd.append(f"-f{cfg}")
    return cmd


def render_config(sample: DocSample) -> str:
    parts: list[str] = []
    if sample.prepend:
        parts.extend(sample.prepend)
        parts.append("")
    parts.append(sample.code.rstrip("\n"))
    if sample.append:
        parts.append("")
        parts.extend(sample.append)
    return "\n".join(parts) + "\n"


def validate_sample(
    sample: DocSample,
    build_dir: Path,
    work_dir: Path,
    rsyslogd: Path,
    timeout: float,
) -> str:
    missing_plugins = [
        plugin for plugin in sample.required_plugins if not plugin_available(build_dir, plugin, rsyslogd, timeout)
    ]
    if missing_plugins:
        print(
            f"SKIP {sample.source}:{sample.line}: missing plugin(s): {', '.join(missing_plugins)}",
            flush=True,
        )
        return "skip"

    cfg = work_dir / f"{sample.label}.conf"
    log = work_dir / f"{sample.label}.log"
    cfg.write_text(render_config(sample), encoding="utf-8")
    try:
        result = subprocess.run(
            rsyslogd_check_command(rsyslogd, build_dir, cfg),
            cwd=build_dir,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            timeout=timeout,
            check=False,
        )
        output = result.stdout
        returncode = result.returncode
    except subprocess.TimeoutExpired as exc:
        output = timeout_output(exc)
        output += f"\nrsyslogd config check timed out after {timeout:g} seconds.\n"
        returncode = 124
    log.write_text(output, encoding="utf-8")
    if returncode != 0:
        print(f"FAIL {sample.source}:{sample.line}: {cfg}", file=sys.stderr)
        print(output, file=sys.stderr)
        return "fail"
    print(f"PASS {sample.source}:{sample.line}", flush=True)
    return "pass"


def main() -> int:
    args = parse_args()
    source_dir = Path(args.source_dir).resolve()
    build_dir = Path(args.build_dir).resolve()
    rsyslogd = Path(args.rsyslogd).resolve() if args.rsyslogd else build_dir / "tools" / "rsyslogd"
    work_dir = Path(args.work_dir).resolve() if args.work_dir else build_dir / "doc" / "build" / "doc-samples"
    timeout = args.timeout

    if not source_dir.is_dir():
        print(f"source directory not found: {source_dir}", file=sys.stderr)
        return 2
    if not rsyslogd.is_file():
        print(f"rsyslogd not found: {rsyslogd}", file=sys.stderr)
        return 2
    if timeout <= 0:
        print("timeout must be greater than zero", file=sys.stderr)
        return 2

    shutil.rmtree(work_dir, ignore_errors=True)
    work_dir.mkdir(parents=True, exist_ok=True)

    try:
        samples = discover_samples(source_dir)
    except ValueError as exc:
        print(exc, file=sys.stderr)
        return 2

    if not samples:
        print("No marked rsyslog documentation samples found.", file=sys.stderr)
        return 1

    counts = {"pass": 0, "skip": 0, "fail": 0}
    for sample in samples:
        outcome = validate_sample(sample, build_dir, work_dir, rsyslogd, timeout)
        counts[outcome] += 1

    print(
        "Doc sample validation summary: "
        f"{counts['pass']} passed, {counts['skip']} skipped, {counts['fail']} failed."
    )
    return 1 if counts["fail"] else 0


if __name__ == "__main__":
    sys.exit(main())
