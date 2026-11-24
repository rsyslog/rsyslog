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
"""Flag GCC __atomic_* / __sync_* builtins used without an atomics guard.

rsyslog supports compilers without GCC atomic builtins (e.g. Solaris
Studio).  configure then leaves HAVE_ATOMIC_BUILTINS undefined and
runtime/atomic.h falls back to mutex-based helpers.  Code must therefore
either use the atomic.h helpers or call the builtins only inside a
preprocessor branch that requires HAVE_ATOMIC_BUILTINS or
HAVE_ATOMIC_BUILTINS64.  A raw builtin outside such a branch breaks
those builds, which normal Linux CI never notices.

The check is preprocessor-aware rather than file-based: guarded raw use
(for example the whole lock-free local queue code) is fine.  It is
deliberately conservative: a branch counts as guarded only when its
condition is a plain conjunction that positively requires one of the
guard macros, or the #else of a plain negated guard.

Pre-existing findings can be listed in devtools/raw-atomics-baseline.txt
so the check can gate new code while old code is being fixed.  Stale
baseline entries are reported as warnings, never as failures.

Usage: devtools/check-raw-atomics.py [--baseline FILE] [path ...]
Without paths, all tracked *.c and *.h files are scanned.
"""

from __future__ import annotations

import argparse
import os
import re
import subprocess
import sys
from collections import Counter
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_BASELINE = ROOT / "devtools/raw-atomics-baseline.txt"
GUARD_MACROS = ("HAVE_ATOMIC_BUILTINS", "HAVE_ATOMIC_BUILTINS64")
BUILTIN_RE = re.compile(r"(?<![A-Za-z0-9_])(__(?:atomic|sync)_[a-z0-9_]+)\s*\(")
DIRECTIVE_RE = re.compile(r"^\s*#\s*([a-z]+)\b(.*)$")
GUARD_ALT = "|".join(GUARD_MACROS)
GUARD_TERM = r"(?:defined\s*\(\s*(?:{0})\s*\)|defined\s+(?:{0})|(?:{0}))".format(GUARD_ALT)
POSITIVE_TERM_RE = re.compile(r"^" + GUARD_TERM + r"$")
NEGATED_TERM_RE = re.compile(r"^!\s*" + GUARD_TERM + r"$")


def strip_comments_and_strings(text: str) -> str:
    """Blank out comments and string/char literals, keeping line structure."""
    out = []
    i = 0
    n = len(text)
    while i < n:
        c = text[i]
        if c == "/" and i + 1 < n and text[i + 1] == "*":
            end = text.find("*/", i + 2)
            end = n if end < 0 else end + 2
            out.append(re.sub(r"[^\n]", " ", text[i:end]))
            i = end
        elif c == "/" and i + 1 < n and text[i + 1] == "/":
            end = text.find("\n", i)
            end = n if end < 0 else end
            out.append(" " * (end - i))
            i = end
        elif c in "\"'":
            j = i + 1
            while j < n and text[j] != c and text[j] != "\n":
                j += 2 if text[j] == "\\" else 1
            j = min(j + 1, n)
            out.append(c + re.sub(r"[^\n]", " ", text[i + 1:j - 1]) + (text[j - 1] if j - 1 > i else ""))
            i = j
        else:
            out.append(c)
            i += 1
    return "".join(out)


def logical_lines(text: str):
    """Yield (first_line_number, joined_text, physical_parts) per logical line."""
    lines = text.split("\n")
    i = 0
    while i < len(lines):
        start = i
        parts = [lines[i]]
        while parts[-1].endswith("\\") and i + 1 < len(lines):
            parts[-1] = parts[-1][:-1]
            i += 1
            parts.append(lines[i])
        yield start + 1, " ".join(parts), parts
        i += 1


def split_conjunction(expr: str) -> list[str] | None:
    """Split a top-level '&&' chain; None if it has a top-level '||'."""
    terms = []
    depth = 0
    cur = []
    i = 0
    while i < len(expr):
        c = expr[i]
        if c == "(":
            depth += 1
        elif c == ")":
            depth -= 1
        if depth == 0 and expr.startswith("||", i):
            return None
        if depth == 0 and expr.startswith("&&", i):
            terms.append("".join(cur).strip())
            cur = []
            i += 2
            continue
        cur.append(c)
        i += 1
    terms.append("".join(cur).strip())
    return terms


def unparen(term: str) -> str:
    """Drop parentheses that enclose the whole term."""
    while term.startswith("(") and term.endswith(")"):
        depth = 0
        for i, c in enumerate(term):
            depth += (c == "(") - (c == ")")
            if depth == 0 and i < len(term) - 1:
                return term
        term = term[1:-1].strip()
    return term


def if_requires_guard(expr: str) -> bool:
    terms = split_conjunction(expr.strip())
    if terms is None:
        return False
    return any(POSITIVE_TERM_RE.match(unparen(t)) for t in terms)


def if_negates_guard(expr: str) -> bool:
    return bool(NEGATED_TERM_RE.match(unparen(expr.strip())))


def scan_file(path: Path, rel: str):
    try:
        raw = path.read_text(encoding="utf-8", errors="replace")
    except OSError as exc:
        print(f"warning: cannot read {rel}: {exc}", file=sys.stderr)
        return
    code = strip_comments_and_strings(raw)
    raw_lines = raw.split("\n")
    # Each frame: [guarded_now, else_branch_guarded]
    stack: list[list[bool]] = []
    for lineno, text, parts in logical_lines(code):
        m = DIRECTIVE_RE.match(text)
        if m:
            directive, arg = m.group(1), m.group(2).strip()
            if directive == "ifdef":
                stack.append([arg in GUARD_MACROS, False])
            elif directive == "ifndef":
                stack.append([False, arg in GUARD_MACROS])
            elif directive == "if":
                stack.append([if_requires_guard(arg), if_negates_guard(arg)])
            elif directive == "elif" and stack:
                stack[-1] = [if_requires_guard(arg), False]
            elif directive == "else" and stack:
                stack[-1] = [stack[-1][1], False]
            elif directive == "endif" and stack:
                stack.pop()
            if directive != "define":
                continue
        if any(frame[0] for frame in stack):
            continue
        for bm in BUILTIN_RE.finditer(text):
            # Report the physical line that holds the builtin.
            physical = lineno
            consumed = 0
            for part in parts[:-1]:
                consumed += len(part) + 1
                if bm.start() < consumed:
                    break
                physical += 1
            source = raw_lines[physical - 1].strip() if physical - 1 < len(raw_lines) else ""
            yield physical, bm.group(1), source


def load_baseline(path: Path) -> Counter:
    entries: Counter = Counter()
    if not path.exists():
        return entries
    for line in path.read_text(encoding="utf-8").splitlines():
        if not line.strip() or line.lstrip().startswith("#"):
            continue
        rel, sep, source = line.partition(": ")
        if not sep:
            print(f"warning: malformed baseline line: {line}", file=sys.stderr)
            continue
        entries[(rel.strip(), source.strip())] += 1
    return entries


def tracked_sources() -> list[str]:
    out = subprocess.run(["git", "ls-files", "-z", "--", "*.c", "*.h"], cwd=ROOT,
                         check=True, capture_output=True).stdout
    return [p for p in out.decode().split("\0") if p]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--baseline", type=Path, default=DEFAULT_BASELINE,
                        help="file with accepted pre-existing findings (default: %(default)s)")
    parser.add_argument("--no-baseline", action="store_true", help="report every finding")
    parser.add_argument("paths", nargs="*", help="C/H files to scan (default: all tracked)")
    args = parser.parse_args()

    if args.paths:
        rels = [os.path.relpath(Path(p).resolve(), ROOT) for p in args.paths if p.endswith((".c", ".h"))]
    else:
        rels = tracked_sources()
    baseline = Counter() if args.no_baseline else load_baseline(args.baseline)
    remaining = Counter(baseline)
    scanned = set(rels)
    annotate = os.environ.get("GITHUB_ACTIONS") == "true"

    failures = 0
    for rel in rels:
        path = ROOT / rel
        if not path.is_file():
            continue
        for lineno, builtin, source in scan_file(path, rel):
            key = (rel, source)
            if remaining[key] > 0:
                remaining[key] -= 1
                continue
            failures += 1
            msg = f"{builtin}() used outside a HAVE_ATOMIC_BUILTINS guard; use runtime/atomic.h helpers or guard it"
            print(f"{rel}:{lineno}: {msg}")
            if annotate:
                print(f"::error file={rel},line={lineno}::{msg}")

    for (rel, source), count in sorted(remaining.items()):
        if count > 0 and rel in scanned:
            print(f"warning: stale baseline entry, please remove it: {rel}: {source}", file=sys.stderr)

    if failures:
        print(f"\n{failures} unguarded atomic builtin use(s) found. Builds without atomic builtins "
              "(e.g. Solaris Studio, --disable-atomic-operations) will fail to compile.", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
