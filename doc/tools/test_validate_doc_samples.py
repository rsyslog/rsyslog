#!/usr/bin/env python3
"""Unit tests for validate-doc-samples.py."""

import importlib.util
import os
import subprocess
import tempfile
import textwrap
import unittest
from pathlib import Path


SCRIPT = Path(__file__).with_name("validate-doc-samples.py")


def load_validator():
    spec = importlib.util.spec_from_file_location("validate_doc_samples", SCRIPT)
    module = importlib.util.module_from_spec(spec)
    assert spec.loader is not None
    spec.loader.exec_module(module)
    return module


class ValidateDocSamplesTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)
        self.source = self.root / "doc" / "source"
        self.source.mkdir(parents=True)
        self.build = self.root / "build"
        (self.build / "tools").mkdir(parents=True)
        (self.build / "runtime" / ".libs").mkdir(parents=True)
        (self.build / ".libs").mkdir(parents=True)
        self.rsyslogd = self.build / "tools" / "rsyslogd"
        self.rsyslogd.write_text(
            "#!/bin/sh\n"
            "config=\n"
            "for arg in \"$@\"; do\n"
            "  case \"$arg\" in -f*) config=${arg#-f};; esac\n"
            "done\n"
            "if [ -n \"$config\" ] && grep -q hang \"$config\"; then sleep 5; fi\n"
            "if [ -n \"$config\" ] && grep -q invalid \"$config\"; then exit 1; fi\n"
            "if [ -n \"$config\" ] && grep -q 'module(load=\"ommissing\")' \"$config\"; then exit 1; fi\n"
            "if [ -n \"$config\" ] && grep -q 'module(load=\"omhanging\")' \"$config\"; then sleep 5; fi\n"
            "exit 0\n",
            encoding="utf-8",
        )
        self.rsyslogd.chmod(0o755)

    def tearDown(self):
        self.tmp.cleanup()

    def write_rst(self, content):
        path = self.source / "sample.rst"
        path.write_text(textwrap.dedent(content), encoding="utf-8")
        return path

    def run_validator(self, *extra_args):
        return subprocess.run(
            [
                "python3",
                str(SCRIPT),
                "--source-dir",
                str(self.source),
                "--build-dir",
                str(self.build),
                "--work-dir",
                str(self.root / "work"),
                *extra_args,
            ],
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            check=False,
        )

    def test_discovers_marked_rsyslog_block(self):
        self.write_rst(
            """
            Title
            =====

            .. rsyslog-doc-sample: validate-config

            .. code-block:: rsyslog

               template(name="example" type="list") {
                   property(name="msg")
               }
            """
        )
        validator = load_validator()
        samples = validator.discover_samples(self.source)
        self.assertEqual(len(samples), 1)
        self.assertIn('property(name="msg")', samples[0].code)

    def test_discovers_nested_marked_rsyslog_block(self):
        self.write_rst(
            """
            * List entry:

              .. rsyslog-doc-sample: validate-config

              .. code-block:: rsyslog

                 template(name="example" type="list") {
                     property(name="msg")
                 }
            """
        )
        validator = load_validator()
        samples = validator.discover_samples(self.source)
        self.assertEqual(len(samples), 1)
        self.assertTrue(samples[0].code.startswith('template(name="example"'))
        self.assertIn('    property(name="msg")', samples[0].code)

    def test_ignores_unmarked_blocks(self):
        self.write_rst(
            """
            .. code-block:: rsyslog

               invalid
            """
        )
        result = self.run_validator()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("No marked rsyslog documentation samples found", result.stderr)

    def test_required_plugin_skip(self):
        self.write_rst(
            """
            .. rsyslog-doc-sample: validate-config
               :require-plugin: ommissing

            .. code-block:: rsyslog

               template(name="example" type="list") {
                   property(name="msg")
               }
            """
        )
        result = self.run_validator()
        self.assertEqual(result.returncode, 0)
        self.assertIn("1 skipped", result.stdout)

    def test_required_plugin_accepts_contrib_module(self):
        contrib_module = self.build / "contrib" / "omexample" / ".libs"
        contrib_module.mkdir(parents=True)
        (contrib_module / "omexample.so").write_text("", encoding="utf-8")
        self.write_rst(
            """
            .. rsyslog-doc-sample: validate-config
               :require-plugin: omexample

            .. code-block:: rsyslog

               template(name="example" type="list") {
                   property(name="msg")
               }
            """
        )
        result = self.run_validator()
        self.assertEqual(result.returncode, 0)
        self.assertIn("1 passed", result.stdout)

    def test_required_plugin_accepts_builtin_module(self):
        self.write_rst(
            """
            .. rsyslog-doc-sample: validate-config
               :require-plugin: ombuiltin

            .. code-block:: rsyslog

               template(name="example" type="list") {
                   property(name="msg")
               }
            """
        )
        result = self.run_validator()
        self.assertEqual(result.returncode, 0)
        self.assertIn("1 passed", result.stdout)

    def test_required_plugin_timeout_is_skipped(self):
        self.write_rst(
            """
            .. rsyslog-doc-sample: validate-config
               :require-plugin: omhanging

            .. code-block:: rsyslog

               template(name="example" type="list") {
                   property(name="msg")
               }
            """
        )
        result = self.run_validator("--timeout", "0.1")
        self.assertEqual(result.returncode, 0)
        self.assertIn("1 skipped", result.stdout)

    def test_validation_failure_is_reported(self):
        self.write_rst(
            """
            .. rsyslog-doc-sample: validate-config

            .. code-block:: rsyslog

               invalid
            """
        )
        result = self.run_validator()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("FAIL sample.rst", result.stderr)

    def test_validation_timeout_is_reported(self):
        self.write_rst(
            """
            .. rsyslog-doc-sample: validate-config

            .. code-block:: rsyslog

               hang
            """
        )
        result = self.run_validator("--timeout", "0.1")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("timed out", result.stderr)
        log = next((self.root / "work").glob("*.log")).read_text(encoding="utf-8")
        self.assertIn("timed out", log)

    def test_prepend_and_append_are_written(self):
        self.write_rst(
            """
            .. rsyslog-doc-sample: validate-config
               :prepend: global(workDirectory="/tmp")
               :append: action(type="omfile" file="/dev/null")

            .. code-block:: rsyslog

               template(name="example" type="list") {
                   property(name="msg")
               }
            """
        )
        result = self.run_validator()
        self.assertEqual(result.returncode, 0)
        config = next((self.root / "work").glob("*.conf")).read_text(encoding="utf-8")
        self.assertIn('global(workDirectory="/tmp")', config)
        self.assertIn('action(type="omfile" file="/dev/null")', config)

    def test_module_search_path_includes_plugin_and_contrib_libs(self):
        plugin_lib = self.build / "plugins" / "imtcp" / ".libs"
        contrib_lib = self.build / "contrib" / "fmhash" / ".libs"
        plugin_lib.mkdir(parents=True)
        contrib_lib.mkdir(parents=True)
        validator = load_validator()
        search_path = validator.module_search_path(self.build).split(os.pathsep)
        self.assertIn(str(self.build / "runtime" / ".libs"), search_path)
        self.assertIn(str(plugin_lib), search_path)
        self.assertIn(str(contrib_lib), search_path)

    def test_rsyslogd_command_omits_empty_module_path(self):
        empty_build = self.root / "empty-build"
        cfg = self.root / "sample.conf"
        cfg.write_text("", encoding="utf-8")
        validator = load_validator()
        cmd = validator.rsyslogd_check_command(self.rsyslogd, empty_build, cfg)
        self.assertNotIn("-M", cmd)
        self.assertEqual(cmd[-1], f"-f{cfg}")


if __name__ == "__main__":
    unittest.main()
