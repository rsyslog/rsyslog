#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Exercise advisory port detection using the scanner's public report.

The fixed-port section must report complete assignment literals, including
syslog's 514, while dynamic values and numeric fragments remain absent. Bare
colon detection keeps its longer-port scope to avoid JSON/slice/message noise. Both search
backends must agree, and findings must retain the advisory zero exit status.
"""

import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


SCANNER = Path(__file__).resolve().parents[1] / "check-test-antipatterns.sh"
BACKENDS = (False, True) if shutil.which("rg") else (True,)


class FixedPortLiterals(unittest.TestCase):
    def scan(self, lines, fallback):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            fixture = root / "ports.sh"
            fixture.write_text("\n".join(lines) + "\n", encoding="utf-8")
            env = os.environ.copy()
            if fallback:
                tools = root / "bin"
                tools.mkdir()
                for name in ("bash", "mktemp", "rm", "find", "sort", "wc", "xargs", "grep"):
                    (tools / name).symlink_to(shutil.which(name))
                env["PATH"] = str(tools)
            result = subprocess.run([str(SCANNER), str(fixture)], env=env,
                                    text=True, capture_output=True, check=False)
            self.assertEqual(0, result.returncode, result.stderr)
            section = result.stdout.split("## Fixed TCP/UDP port literals\n", 1)[1].split("\n## ", 1)[0]
            return [line.split(":", 2)[2] for line in section.splitlines()
                    if line.startswith(str(fixture) + ":")]

    def test_three_digit_and_existing_longer_literals(self):
        lines = ['input(port="514")', 'port=514', 'PORT=514',
                 'listener_port=514', 'port="5140"', 'PORT=6514', 'port="65535"',
                 'ES_PORT=19200', 'serverport=19200', 'target="localhost:6514"']
        for fallback in BACKENDS:
            with self.subTest(fallback=fallback):
                self.assertEqual(lines, self.scan(lines, fallback))

    def test_dynamic_ports_and_numeric_fragments(self):
        lines = ['port="0"', 'port="$PORT"', 'port="${PORT}"', 'target="localhost:$PORT"',
                 'port="514suffix"', 'port=514123', 'port="514.5"', 'report=514', 'support=514',
                 'target="localhost:514suffix"', 'target="localhost:514123"',
                 'target="localhost:514"', '{"status":201}', '${VALUE:0:128}', '2001:db8::514']
        for fallback in BACKENDS:
            with self.subTest(fallback=fallback):
                self.assertEqual([], self.scan(lines, fallback))


if __name__ == "__main__":
    unittest.main()
