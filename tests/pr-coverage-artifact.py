#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Regression oracle: hostile artifacts cannot write outside their destination.

Each fixture runs in its own temporary directory. Acceptance is checked against
an exact normalized report; rejection must leave no report and preserve the
outside sentinel. Size limits are exercised without timing or network oracles.
"""

import importlib.util
from pathlib import Path
import stat
import sys
import tempfile
import unittest
import warnings
import zipfile

sys.dont_write_bytecode = True
SPEC = importlib.util.spec_from_file_location(
    "coverage_validator", Path(__file__).resolve().parents[1] / "devtools/validate-pr-coverage.py")
VALIDATOR = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(VALIDATOR)
REPORT = b"TN:\nSF:/rsyslog/runtime/msg.c\nDA:12,3\nLF:1\nLH:1\nend_of_record\n"


class ArtifactBoundary(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="rsyslog-coverage-test-")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.archive = self.root / "report.zip"
        self.output = self.root / "coverage.info"
        self.sentinel = self.root / "sentinel"
        self.sentinel.write_text("unchanged", encoding="utf-8")

    def bundle(self, members):
        with warnings.catch_warnings():
            warnings.simplefilter("ignore", UserWarning)
            with zipfile.ZipFile(self.archive, "w", compression=zipfile.ZIP_DEFLATED) as archive:
                for name, data in members:
                    archive.writestr(name, data)

    def reject(self, members):
        self.bundle(members)
        with self.assertRaises((ValueError, zipfile.BadZipFile)):
            VALIDATOR.validate(self.archive, self.output)
        self.assertFalse(self.output.exists(), "invalid input created a report")
        self.assertEqual(self.sentinel.read_text(encoding="utf-8"), "unchanged")

    def test_valid_absolute_and_relative_sources(self):
        expected = REPORT.replace(b"/rsyslog/", b"").decode("utf-8")
        for report in (REPORT, expected.encode("utf-8")):
            with self.subTest(report=report):
                self.bundle([("coverage.info", report)])
                VALIDATOR.validate(self.archive, self.output)
                self.assertEqual(self.output.read_text(encoding="utf-8"), expected)

    def test_docs_root_normalization_and_profile_isolation(self):
        # Hosted docs sources come from the unpacked doc-builder tree. Only
        # this trusted root may be stripped; sibling/root substitutions fail.
        docs_root = "/home/runner/work/rsyslog/rsyslog/doc-builder"
        report = REPORT.replace(b"/rsyslog", docs_root.encode("utf-8"))
        self.bundle([("coverage.info", report)])
        VALIDATOR.validate(self.archive, self.output, docs_root)
        self.assertEqual(self.output.read_bytes(), REPORT.replace(b"/rsyslog/", b""))
        self.output.unlink()
        for root, hostile in (("/rsyslog", report), (docs_root, REPORT),
                              (docs_root, report.replace(b"doc-builder/", b"doc-builder-evil/")),
                              (docs_root, report.replace(b"runtime/msg.c", b"../sentinel"))):
            with self.subTest(root=root, hostile=hostile):
                self.bundle([("coverage.info", hostile)])
                with self.assertRaisesRegex(ValueError, "unsafe source path"):
                    VALIDATOR.validate(self.archive, self.output, root)
                self.assertFalse(self.output.exists())
                self.assertEqual(self.sentinel.read_text(encoding="utf-8"), "unchanged")

    def test_archive_paths_and_extra_members_never_extract(self):
        for name in ("../sentinel", "sub/coverage.info", "/coverage.info", "coverage.info/"):
            with self.subTest(name=name):
                self.reject([(name, REPORT)])
        for members in ([], [("coverage.info", REPORT), ("../sentinel", b"changed")],
                        [("coverage.info", REPORT), ("coverage.info", REPORT)]):
            with self.subTest(members=len(members)):
                self.reject(members)

    def test_zip_links_are_not_reports(self):
        link = zipfile.ZipInfo("coverage.info")
        link.create_system = 3
        link.external_attr = (stat.S_IFLNK | 0o777) << 16
        self.reject([(link, b"sentinel")])

    def test_unsafe_lcov_source_paths(self):
        for source in (b"/etc/passwd", b"../sentinel", b"/rsyslog/../sentinel",
                       b"runtime/../../sentinel", b"C:/sentinel", b"runtime\\sentinel", b"-option"):
            with self.subTest(source=source):
                self.reject([("coverage.info", REPORT.replace(b"/rsyslog/runtime/msg.c", source))])

    def test_non_lcov_binary_and_incomplete_reports(self):
        for report in (b"", b"\xff", REPORT + b"\x00", b"#!/bin/sh\necho changed\n",
                       REPORT.replace(b"end_of_record\n", b""), REPORT + b"unknown:value\n",
                       REPORT.replace(b"TN:", b"TN:" + b"x" * 16385)):
            with self.subTest(report_size=len(report)):
                self.reject([("coverage.info", report)])

    def test_compressed_bomb_exceeds_uncompressed_limit(self):
        # Highly compressible bytes stay below the archive budget while proving
        # the separate uncompressed bound is enforced before report parsing.
        self.reject([("coverage.info", b"x" * (VALIDATOR.MAX_REPORT + 1))])
        self.assertLess(self.archive.stat().st_size, VALIDATOR.MAX_ARCHIVE)

    def test_archive_limit_is_checked_before_zip_parsing(self):
        with self.archive.open("wb") as archive:
            archive.seek(VALIDATOR.MAX_ARCHIVE)
            archive.write(b"x")
        with self.assertRaisesRegex(ValueError, "archive is too large"):
            VALIDATOR.validate(self.archive, self.output)
        self.assertFalse(self.output.exists())


if __name__ == "__main__":
    unittest.main()
