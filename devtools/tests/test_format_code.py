#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Exercise the real format gate in isolated Git histories without a formatter dependency.

The formatter stand-in records selected paths and rejects the known unformatted
fixture. A bad comparison ref must fail before calling it; valid refs must still
cover committed, staged, unstaged, and untracked C/H files. A genuinely unchanged
history must succeed without a formatter invocation. No test uses network refs.
"""

import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]


class FormatCodeTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.repo = Path(self.temp.name) / "repo"
        self.repo.mkdir()
        self.bin = Path(self.temp.name) / "bin"
        self.bin.mkdir()
        self.log = Path(self.temp.name) / "formatter.log"
        self.env = {key: value for key, value in os.environ.items() if not key.startswith("GIT_")}
        self.env.pop("RSYSLOG_LOCAL_VALIDATION_BASE", None)
        self.env["PATH"] = str(self.bin) + os.pathsep + self.env["PATH"]
        self.env["FORMATTER_LOG"] = str(self.log)
        self.git("init", "-q", "-b", "main")
        self.git("config", "user.name", "Format gate test")
        self.git("config", "user.email", "format-test@example.invalid")
        self.git("config", "core.hooksPath", "/dev/null")
        self.git("config", "commit.gpgsign", "false")
        (self.repo / "devtools").mkdir()
        for name in ("format-code.sh", "list-git-changed-c-h-files.sh"):
            shutil.copy2(ROOT / "devtools" / name, self.repo / "devtools" / name)
        (self.repo / ".clang-format").write_text("BasedOnStyle: LLVM\n")
        self.git("add", ".")
        self.git("commit", "-qm", "baseline")
        self.git("tag", "test-base")
        self.git("update-ref", "refs/remotes/origin/main", "HEAD")
        formatter = self.bin / "clang-format-18"
        formatter.write_text('''#!/bin/bash
printf '%s\\n' "$@" > "$FORMATTER_LOG"
for file in "$@"; do
    case "$file" in
        *.c|*.h) if grep -q UNFORMATTED "$file"; then exit 1; fi ;;
    esac
done
''')
        formatter.chmod(0o755)

    def run_command(self, *args, env=None):
        return subprocess.run(args, cwd=self.repo, env=env or self.env, text=True, capture_output=True)

    def git(self, *args):
        result = self.run_command("git", *args)
        self.assertEqual(result.returncode, 0, result.stderr)
        return result.stdout.strip()

    def add_unformatted(self, name):
        (self.repo / name).write_text("int main(){return 0;} /* UNFORMATTED */\n")

    def commit_unformatted(self):
        self.add_unformatted("committed.c")
        self.git("add", "committed.c")
        self.git("commit", "-qm", "unformatted branch change")

    def check_format(self, base=None):
        env = self.env.copy()
        if base is not None:
            env["RSYSLOG_LOCAL_VALIDATION_BASE"] = base
        return self.run_command("./devtools/format-code.sh", "--git-changed", "--check", env=env)

    def assert_bad_base(self, base, expected_ref):
        result = self.check_format(base)
        self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn(expected_ref, result.stderr)
        self.assertFalse(self.log.exists(), "invalid base must not invoke a partial formatter check")

    def test_explicit_missing_base_fails_before_checking(self):
        self.commit_unformatted()
        self.assert_bad_base("misspelled-base", "misspelled-base")

    def test_missing_default_base_fails_before_checking(self):
        self.commit_unformatted()
        self.git("update-ref", "-d", "refs/remotes/origin/main")
        self.assert_bad_base(None, "origin/main")

    def test_invalid_base_does_not_fall_back_to_local_edits(self):
        self.commit_unformatted()
        self.add_unformatted("untracked.h")
        self.assert_bad_base("misspelled-base", "misspelled-base")

    def test_valid_explicit_base_checks_committed_changes(self):
        self.commit_unformatted()
        result = self.check_format("test-base")
        self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("committed.c", self.log.read_text().splitlines())

    def test_valid_default_base_checks_committed_changes(self):
        self.commit_unformatted()
        result = self.check_format()
        self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("committed.c", self.log.read_text().splitlines())

    def test_valid_base_includes_all_local_change_states(self):
        (self.repo / "unstaged.h").write_text("/* baseline */\n")
        self.git("add", "unstaged.h")
        self.git("commit", "-qm", "tracked header")
        self.git("tag", "local-base")
        self.add_unformatted("unstaged.h")
        self.add_unformatted("staged.c")
        self.git("add", "staged.c")
        self.add_unformatted("untracked space.c")
        env = self.env | {"RSYSLOG_LOCAL_VALIDATION_BASE": "local-base"}
        result = self.run_command("./devtools/list-git-changed-c-h-files.sh", env=env)
        self.assertEqual(result.returncode, 0, result.stderr)
        expected = {"unstaged.h", "staged.c", "untracked space.c"}
        self.assertEqual(set(result.stdout.splitlines()), expected)
        result = self.check_format("local-base")
        self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(set(self.log.read_text().splitlines()) & expected, expected)

    def test_no_changes_really_pass_without_formatter(self):
        result = self.check_format("test-base")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertFalse(self.log.exists())

    def test_selector_failure_is_not_an_empty_success(self):
        helper = self.repo / "devtools/list-git-changed-c-h-files.sh"
        helper.write_text("#!/bin/bash\necho 'selector failed deliberately' >&2\nexit 2\n")
        result = self.check_format("test-base")
        self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("selector failed deliberately", result.stderr)
        self.assertFalse(self.log.exists())


if __name__ == "__main__":
    unittest.main()
