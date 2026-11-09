#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Check experiment provenance and the setup/test/package-failure boundary."""
import hashlib
import importlib.util
import io
import json
from pathlib import Path
import tempfile
import unittest
from unittest import mock

SPEC = importlib.util.spec_from_file_location("repro", Path(__file__).resolve().parents[1] /
                                              "debian-repro-experiment.py")
REPRO = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(REPRO)
FIXTURE = (b"-----BEGIN PGP SIGNED MESSAGE-----\nHash: SHA512\n\n"
           b"Source: rsyslog\nVersion: 8.2610.0-3\nBuild-Architecture: amd64\n"
           b"Environment:\n DEB_BUILD_OPTIONS=\"parallel=6\"\n"
           b"\n-----BEGIN PGP SIGNATURE-----\nfixture signature\n-----END PGP SIGNATURE-----\n")


class Process:
    def __init__(self, output, code):
        self.stdout = io.StringIO(output)
        self.code = code

    def wait(self):
        return self.code


class Experiment(unittest.TestCase):
    def execute(self, output, code, extra=()):
        with tempfile.TemporaryDirectory() as directory:
            out = Path(directory)
            with mock.patch.object(REPRO.urllib.request, "urlopen", return_value=io.BytesIO(FIXTURE)), \
                    mock.patch.dict(REPRO.INPUTS, {"amd64": hashlib.sha256(FIXTURE).hexdigest()}), \
                    mock.patch.object(REPRO.subprocess, "check_output", return_value="amd64\n"), \
                    mock.patch.object(REPRO.subprocess, "run"), \
                    mock.patch.object(REPRO.subprocess, "Popen", return_value=Process(output, code)), \
                    mock.patch.object(REPRO, "phase") as phase, \
                    mock.patch.object(REPRO.sys, "argv", ["repro", "--arch", "amd64", "--output", str(out), *extra]):
                with mock.patch.dict(REPRO.os.environ, {"RSYSLOG_FLAKE_EVIDENCE_ROOT": str(out / "evidence")}):
                    result = REPRO.main()
                metadata = json.loads((out / "rebuild.json").read_text())
                inputs = {p.name: p.read_bytes() for p in out.glob("*.buildinfo")}
                return result, metadata, phase.call_args_list, inputs

    def test_setup_failure_is_not_a_test_phase(self):
        result, metadata, phases, _ = self.execute("configure: error: missing dependency\n", 2)
        self.assertEqual(result, 2)
        self.assertFalse(metadata["test_phase_started"])
        self.assertEqual(phases, [])

    def test_failed_test_creates_failure_phase(self):
        _, metadata, phases, _ = self.execute("FAIL: example.sh\n# TOTAL: 1\n# FAIL: 1\n# ERROR: 0\n", 2)
        self.assertEqual(metadata["test_counts"]["FAIL"], 1)
        self.assertEqual(phases, [mock.call("begin"), mock.call("end", 1)])

    def test_package_failure_after_pass_is_not_a_flake(self):
        result, _, phases, _ = self.execute("PASS: example.sh\n# TOTAL: 1\n# FAIL: 0\n# ERROR: 0\n"
                                            "package checksum mismatch\n", 1)
        self.assertEqual(result, 1)
        self.assertEqual(phases, [mock.call("begin"), mock.call("end", 0)])

    def test_parallelism_override_preserves_original(self):
        _, metadata, _, inputs = self.execute("", 0, ["--parallelism", "2", "--download-only"])
        self.assertEqual(inputs["rsyslog_8.2610.0-3_amd64.buildinfo"], FIXTURE)
        derived = inputs["derived-parallel2-rsyslog_8.2610.0-3_amd64.buildinfo"]
        self.assertNotIn(b"PGP SIGNATURE", derived)
        self.assertIn(b'DEB_BUILD_OPTIONS="parallel=2"', derived)
        self.assertTrue(metadata["derived_unsigned_input"])
        self.assertEqual(metadata["effective_buildinfo_sha256"], hashlib.sha256(derived).hexdigest())


if __name__ == "__main__":
    unittest.main()
