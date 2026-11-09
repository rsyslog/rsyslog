#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Check experiment provenance and the setup/test/package-failure boundary."""
import contextlib
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
        self.pid = 424242
        self.waited = []

    def wait(self, timeout=None):
        self.waited.append(timeout)
        return self.code

    def poll(self):
        return self.code


class Experiment(unittest.TestCase):
    def execute(self, output, code, extra=(), process=None):
        process = process or Process(output, code)
        with tempfile.TemporaryDirectory() as directory:
            out = Path(directory)
            with mock.patch.object(REPRO.urllib.request, "urlopen", return_value=io.BytesIO(FIXTURE)), \
                    mock.patch.dict(REPRO.INPUTS, {"amd64": hashlib.sha256(FIXTURE).hexdigest()}), \
                    mock.patch.object(REPRO.subprocess, "check_output", return_value="amd64\n"), \
                    mock.patch.object(REPRO.subprocess, "run"), \
                    mock.patch.object(REPRO.subprocess, "Popen", return_value=process), \
                    mock.patch.object(REPRO, "phase") as phase, \
                    mock.patch.object(REPRO.sys, "argv", ["repro", "--arch", "amd64", "--output", str(out), *extra]):
                with mock.patch.dict(REPRO.os.environ, {"RSYSLOG_FLAKE_EVIDENCE_ROOT": str(out / "evidence")}):
                    with contextlib.redirect_stdout(io.StringIO()):
                        result = REPRO.main()
                metadata = json.loads((out / "rebuild.json").read_text())
                inputs = {p.name: p.read_bytes() for p in out.glob("*.buildinfo")}
                if metadata.get("test_phase_started"):
                    provenance = json.loads((out / "evidence/logs/debian-rebuild-provenance.log").read_text())
                    self.assertEqual(provenance["source_version"], "8.2610.0-3")
                    self.assertEqual(provenance["buildinfo_sha256"], metadata["buildinfo_sha256"])
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

    def test_nested_summary_cannot_hide_interrupted_outer_test(self):
        result, metadata, phases, _ = self.execute(
            "PASS: nested.sh\n# TOTAL: 1\n# FAIL: 0\n# ERROR: 0\nTest: ./blocked.sh\n", 2)
        self.assertEqual(result, 2)
        self.assertFalse(metadata["test_summary_complete"])
        self.assertEqual(phases, [mock.call("begin"), mock.call("end", 1)])

    def test_compiled_unit_failure_starts_phase(self):
        _, _, phases, _ = self.execute("FAIL: runtime_unit_linkedlist\n# TOTAL: 1\n# FAIL: 1\n# ERROR: 0\n", 2)
        self.assertEqual(phases, [mock.call("begin"), mock.call("end", 1)])

    def test_unexpected_pass_is_not_a_successful_phase(self):
        _, metadata, phases, _ = self.execute(
            "XPASS: unexpected.sh\n# TOTAL: 1\n# FAIL: 0\n# XPASS: 1\n# ERROR: 0\n", 1)
        self.assertEqual(metadata["test_counts"]["XPASS"], 1)
        self.assertEqual(phases, [mock.call("begin"), mock.call("end", 1)])

    def test_output_error_stops_owned_process_and_ends_phase(self):
        def broken_output():
            yield "PASS: started.sh\n"
            raise OSError("fixture output failure")
        process = Process("", None)
        process.stdout = broken_output()
        with mock.patch.object(REPRO.os, "killpg") as kill:
            result, metadata, phases, _ = self.execute("", 2, process=process)
        self.assertEqual(result, 2)
        self.assertIn("fixture output failure", metadata["execution_error"])
        kill.assert_called_once_with(process.pid, REPRO.signal.SIGTERM)
        self.assertEqual(process.waited, [10])
        self.assertEqual(phases, [mock.call("begin"), mock.call("end", 1)])

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
