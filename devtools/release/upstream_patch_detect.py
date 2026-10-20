#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
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
"""Detect downstream baseline patches that current upstream already contains.

Distributions mostly carry rsyslog patches that are backports of upstream
commits, typically after a security release.  Daily-stable builds package
current main, so such a patch neither applies nor is needed.  This helper
recognizes those patches from the upstream references in their headers, so a
routine distribution backport does not stop every nightly build until a
policy file is edited by hand.

Accepted evidence, checked against the history of the packaged source:

- a ``git format-patch`` header (``From <sha> Mon Sep 17 00:00:00 2001``),
- ``(cherry picked from commit <sha>)``,
- ``[partial] backport of [upstream] commit <sha>``,
- a DEP-3 ``Origin:`` or ``Applied-Upstream:`` field with an upstream commit
  URL or a ``commit:<sha>`` token,
- failing all references, a patch subject identical to an upstream subject.

Every referenced commit must be an ancestor of the packaged source and must
not have been reverted there (by a ``Revert "<subject>"`` commit).  Free-text
commit mentions are deliberately ignored: a fix for a regression usually names
the commit that introduced the regression, and that commit is in main even
though the fix may not be.

The commit index is ``git log --format='%H %s'`` output for the packaged
source revision.

CLI: ``upstream_patch_detect.py <commit-index> <patch-file>`` prints the
evidence and exits 0 when the patch is integrated, 1 when it is not.
"""

import pathlib
import re
import sys

MIN_SUBJECT_LENGTH = 20
MIN_ABBREV_LENGTH = 7

_FORMAT_PATCH_FROM = re.compile(r"^From ([0-9a-f]{40}) Mon Sep 17 00:00:00 2001$", re.M)
_CHERRY_PICK = re.compile(r"\(cherry picked from commit ([0-9a-f]{7,40})\)", re.I)
_BACKPORT = re.compile(
    r"\b(?:partial\s+)?backport(?:ed)?\s+(?:of|from)\s+(?:upstream\s+)?commit\s+([0-9a-f]{7,40})\b",
    re.I,
)
_DEP3_FIELD = re.compile(r"^(?:Origin|Applied-Upstream):(.*)$", re.I | re.M)
# Only an explicit commit URL or a DEP-3 "commit:<sha>" token counts; free text
# such as "fixes a regression from commit X" names the wrong commit.
_DEP3_COMMIT = re.compile(
    r"(?:github\.com/rsyslog/rsyslog/commit/|(?:^|,)\s*commit:\s*)([0-9a-f]{7,40})\b", re.I
)
_SUBJECT = re.compile(r"^Subject:[ \t]*(.*(?:\n[ \t]+.*)*)", re.I | re.M)
_PATCH_TAG = re.compile(r"^\[PATCH[^\]]*\]\s*", re.I)
_HEADER_END = re.compile(r"^(?:---$|--- |diff |Index: |===)", re.M)


def load_commit_index(path):
    """Return ({sha: subject}, {subject: sha}) from ``git log --format='%H %s'``."""
    by_sha = {}
    by_subject = {}
    for line in pathlib.Path(path).read_text(encoding="utf-8", errors="replace").splitlines():
        sha, _, subject = line.partition(" ")
        if not re.fullmatch(r"[0-9a-f]{40}", sha):
            continue
        by_sha[sha] = subject
        by_subject.setdefault(subject, sha)
    return by_sha, by_subject


def patch_header(text):
    match = _HEADER_END.search(text)
    return text[: match.start()] if match else text


def referenced_commits(header):
    refs = []
    refs += _FORMAT_PATCH_FROM.findall(header)
    refs += _CHERRY_PICK.findall(header)
    refs += _BACKPORT.findall(header)
    for field in _DEP3_FIELD.findall(header):
        refs += _DEP3_COMMIT.findall(field)
    return list(dict.fromkeys(ref.lower() for ref in refs))


def patch_subject(header):
    match = _SUBJECT.search(header)
    if not match:
        return ""
    subject = " ".join(match.group(1).split())
    return _PATCH_TAG.sub("", subject).strip()


def is_reverted(subject, by_subject):
    """A later ``git revert`` of the commit means main no longer contains it."""
    return f'Revert "{subject}"' in by_subject


def resolve(ref, by_sha):
    if ref in by_sha:
        return ref
    if len(ref) < MIN_ABBREV_LENGTH:
        return None
    matches = [sha for sha in by_sha if sha.startswith(ref)]
    return matches[0] if len(matches) == 1 else None


def detect(patch_text, by_sha, by_subject):
    """Return (integrated, evidence) for one patch."""
    header = patch_header(patch_text)
    refs = referenced_commits(header)
    if refs:
        resolved = [(ref, resolve(ref, by_sha)) for ref in refs]
        missing = [ref for ref, sha in resolved if sha is None]
        if missing:
            return False, "references commits not in packaged upstream: " + ", ".join(missing)
        reverted = [sha for _, sha in resolved if is_reverted(by_sha[sha], by_subject)]
        if reverted:
            return False, "references commits reverted upstream: " + ", ".join(sha[:12] for sha in reverted)
        return True, "; ".join(f"upstream commit {sha[:12]} ({by_sha[sha]})" for _, sha in resolved)

    subject = patch_subject(header)
    if len(subject) >= MIN_SUBJECT_LENGTH and subject in by_subject and \
            not is_reverted(subject, by_subject):
        sha = by_subject[subject]
        return True, f"subject matches upstream commit {sha[:12]} ({subject})"
    return False, "no upstream commit reference or matching subject"


def main():
    if len(sys.argv) != 3:
        print("usage: upstream_patch_detect.py <commit-index> <patch-file>", file=sys.stderr)
        return 2
    by_sha, by_subject = load_commit_index(sys.argv[1])
    text = pathlib.Path(sys.argv[2]).read_text(encoding="utf-8", errors="replace")
    integrated, evidence = detect(text, by_sha, by_subject)
    print(evidence)
    return 0 if integrated else 1


if __name__ == "__main__":
    sys.exit(main())
