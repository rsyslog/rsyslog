#!/usr/bin/env python3

import importlib.util
import json
import os
import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
PHASE = ROOT / "devtools/ci-flake-phase.sh"
PREPARE = ROOT / "devtools/prepare-flake-evidence.py"
COLLECT = ROOT / "devtools/collect-flake-evidence.py"


def load_collector():
    spec = importlib.util.spec_from_file_location("collect_flake_evidence", COLLECT)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class FlakeEvidenceTests(unittest.TestCase):
    def github_env(self):
        return {
            "GITHUB_REPOSITORY": "rsyslog/rsyslog",
            "GITHUB_WORKFLOW": "check",
            "GITHUB_RUN_ID": "123",
            "GITHUB_RUN_ATTEMPT": "2",
            "GITHUB_JOB": "ci",
            "GITHUB_SHA": "a" * 40,
            "GITHUB_EVENT_NAME": "pull_request",
            "GITHUB_SERVER_URL": "https://github.com",
        }

    def test_success_and_failure_markers(self):
        with tempfile.TemporaryDirectory() as temp:
            env = os.environ | {"RSYSLOG_FLAKE_EVIDENCE_ROOT": str(Path(temp) / "evidence")}
            success = subprocess.run(
                [str(PHASE), "run", "success-check", "automake", "--", "true"], env=env,
                text=True, capture_output=True,
            )
            failure = subprocess.run(
                [str(PHASE), "run", "failed-oracle", "custom", "--", "false"], env=env,
                text=True, capture_output=True,
            )
            self.assertEqual(0, success.returncode)
            self.assertEqual(1, failure.returncode)
            self.assertIn("status=success", (Path(temp) / "evidence/phases/success-check.phase").read_text())
            self.assertIn("status=failure", (Path(temp) / "evidence/phases/failed-oracle.phase").read_text())

    def test_prepare_ignores_pre_test_failure(self):
        with tempfile.TemporaryDirectory() as temp:
            completed = subprocess.run(
                [str(PREPARE)], cwd=temp, env=os.environ | self.github_env(),
                text=True, capture_output=True,
            )
            self.assertEqual(0, completed.returncode)
            self.assertEqual("eligible=false", completed.stdout.strip())
            self.assertFalse((Path(temp) / "flake-evidence").exists())

    def test_prepare_emits_custom_failure_manifest(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            env = os.environ | self.github_env()
            subprocess.run([str(PHASE), "begin", "metrics-oracle", "custom"], cwd=temp, env=env, check=True)
            subprocess.run([str(PHASE), "end", "metrics-oracle", "custom", "1"], cwd=temp, env=env, check=True)
            completed = subprocess.run([str(PREPARE)], cwd=temp, env=env, text=True, capture_output=True)
            self.assertEqual(0, completed.returncode, completed.stderr)
            manifest = json.loads((root / "flake-evidence/flake-evidence.json").read_text())
            self.assertEqual("failure", manifest["entries"][0]["phases"][0]["status"])
            self.assertIn("eligible=true", completed.stdout)

    def test_prepare_rejects_distcheck_build_failure(self):
        with tempfile.TemporaryDirectory() as temp:
            env = os.environ | self.github_env()
            subprocess.run([str(PHASE), "begin", "distcheck", "distcheck"], cwd=temp, env=env, check=True)
            subprocess.run([str(PHASE), "end", "distcheck", "distcheck", "2"], cwd=temp, env=env, check=True)
            completed = subprocess.run([str(PREPARE)], cwd=temp, env=env, text=True, capture_output=True)
            self.assertEqual("eligible=false", completed.stdout.strip())

    def test_fallback_parser(self):
        collector = load_collector()
        custom = collector.phases_from_log(
            "RSYSLOG_FLAKE_PHASE_BEGIN name=oracle type=custom\n"
            "RSYSLOG_FLAKE_PHASE_END name=oracle type=custom status=failure exit_code=1\n",
            "job",
        )
        interrupted = collector.phases_from_log(
            "RSYSLOG_FLAKE_PHASE_BEGIN name=check type=automake\n", "job"
        )
        success_then_late_failure = collector.phases_from_log(
            "RSYSLOG_FLAKE_PHASE_BEGIN name=check type=automake\n"
            "RSYSLOG_FLAKE_PHASE_END name=check type=automake status=success exit_code=0\n"
            "# FAIL: 0\ncoverage failed\n",
            "job",
        )
        legacy_log = collector.normalize_log(
            "2026-07-22T00:00:00.123Z # FAIL: 1\n"
            "2026-07-22T00:00:00.124Z FAIL: old-test.sh\n"
        )
        legacy = collector.phases_from_log(legacy_log, "old job")
        self.assertEqual("failure", custom[0]["status"])
        self.assertEqual("interrupted", interrupted[0]["status"])
        self.assertEqual([], success_then_late_failure)
        self.assertEqual("legacy-old-job", legacy[0]["name"])


if __name__ == "__main__":
    unittest.main()
