#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Read one bounded, untrusted LCOV artifact without extracting archive paths.

This is a data boundary, not coverage attestation: a contributor can fabricate
coverage for their own PR. Only the trusted caller supplies commit/PR metadata.
"""

import argparse
from pathlib import Path, PurePosixPath
import stat
import zipfile

MAX_ARCHIVE = 10 * 1024 * 1024
MAX_REPORT = 32 * 1024 * 1024


def validate(archive, destination, source_root="/rsyslog"):
    """Reject traversal, links, extra members, binary data and invalid SF paths."""
    if archive.stat().st_size > MAX_ARCHIVE:
        raise ValueError("coverage archive is too large")
    with zipfile.ZipFile(archive) as bundle:
        members = bundle.infolist()
        if len(members) != 1:
            raise ValueError("coverage archive must contain exactly one report")
        member = members[0]
        mode = member.external_attr >> 16
        if (member.filename != "coverage.info" or member.is_dir()
                or stat.S_IFMT(mode) not in (0, stat.S_IFREG)
                or member.flag_bits & 1 or not 0 < member.file_size <= MAX_REPORT):
            raise ValueError("invalid coverage archive member")
        with bundle.open(member) as source:
            data = source.read(MAX_REPORT + 1)
        if len(data) != member.file_size or len(data) > MAX_REPORT:
            raise ValueError("coverage report size mismatch")
    report = data.decode("utf-8")
    if any(ord(char) < 32 and char not in "\n\t" for char in report):
        raise ValueError("coverage report contains control characters")
    # Only the trusted caller supplies this root; never infer it from LCOV.
    prefix = str(PurePosixPath(source_root)) + "/"
    normalized = []
    sources = records = 0
    for line in report.splitlines():
        if len(line) > 16384:
            raise ValueError("coverage report line is too long")
        if line.startswith("SF:"):
            source = line[3:]
            if source.startswith(prefix):
                source = source[len(prefix):]
            path = PurePosixPath(source)
            if (not source or path.is_absolute() or ".." in path.parts
                    or "\\" in source or ":" in source or source.startswith("-")):
                raise ValueError("coverage report has an unsafe source path")
            line = "SF:" + source
            sources += 1
        elif line == "end_of_record":
            records += 1
        elif not line.startswith(("TN:", "FN:", "FNDA:", "FNF:", "FNH:", "DA:",
                                  "LF:", "LH:", "BRDA:", "BRF:", "BRH:")):
            raise ValueError("unexpected LCOV record")
        normalized.append(line)
    if not sources or sources != records:
        raise ValueError("coverage report has no complete source records")
    destination.write_text("\n".join(normalized) + "\n", encoding="utf-8")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("archive", type=Path)
    parser.add_argument("destination", type=Path)
    parser.add_argument("--source-root", default="/rsyslog", help="Trusted coverage build root")
    args = parser.parse_args()
    validate(args.archive, args.destination, args.source_root)


if __name__ == "__main__":
    main()
