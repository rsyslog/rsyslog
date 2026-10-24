#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Guard packaging sequencing, not the upstream concurrent-writer race.

The real distro helpers run with external build-tool adapters. Those adapters
record whether every generated output is complete when parallel consumers start,
plus normal configure/build/install events. The separate real dependency/package
build is required to validate Qpid itself; this fixture fabricates no compiler
failure and makes no scheduler or timing assumption.
"""
import json
import os
from pathlib import Path
import subprocess
import tarfile
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
OUTPUTS = ["c/src/protocol.h", "c/src/encodings.h", "include/proton/version.h",
           "c/src/core/frame_generators.c", "c/src/core/frame_generators.h",
           "c/src/core/frame_consumers.c", "c/src/core/frame_consumers.h"]
ADAPTER = r'''#!/usr/bin/env python3
import json
import os
from pathlib import Path
import sys

args = sys.argv[1:]
trace = Path(os.environ["QPID_TEST_TRACE"])
outputs = json.loads(os.environ["QPID_TEST_OUTPUTS"])
kind = Path(sys.argv[0]).name
if "generated_c_files" in args:
    for name in outputs:
        path = Path(name)
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text("complete\n")
    event = {"phase": "generation", "complete": True}
elif kind == "cmake":
    assert (Path("..") / "CMakeLists.txt").is_file()
    assert "-DBUILD_STATIC_LIBS=ON" in args
    event = {"phase": "configure"}
elif "install" in args:
    destination = next(arg.split("=", 1)[1] for arg in args if arg.startswith("DESTDIR="))
    artifact = Path(destination) / "usr/lib/qpid-fixture"
    artifact.parent.mkdir(parents=True, exist_ok=True)
    artifact.write_text("installed\n")
    event = {"phase": "install"}
else:
    complete = all(Path(name).is_file() and Path(name).read_text() == "complete\n" for name in outputs)
    event = {"phase": "parallel-consumers", "complete": complete, "jobs": args}
with trace.open("a") as stream:
    stream.write(json.dumps(event) + "\n")
'''


class GenerationOrder(unittest.TestCase):
    def test_generated_outputs_precede_parallel_consumers(self):
        for suite in ["jammy", "noble"]:
            with self.subTest(suite=suite), tempfile.TemporaryDirectory() as directory:
                work = Path(directory)
                archive_dir = work / "debian/qpid-proton"
                archive_dir.mkdir(parents=True)
                source = work / "fixture-source/qpid-proton-0.40.0"
                source.mkdir(parents=True)
                (source / "CMakeLists.txt").write_text("# Build-tool adapter fixture, not Qpid source.\n")
                with tarfile.open(str(archive_dir / "qpid-proton-0.40.0.tar.gz"), "w:gz") as archive:
                    archive.add(str(source), arcname="qpid-proton-0.40.0")
                tools = work / "tools"
                tools.mkdir()
                for name in ["cmake", "make"]:
                    adapter = tools / name
                    adapter.write_text(ADAPTER)
                    adapter.chmod(0o755)
                trace = work / "events.jsonl"
                destination = work / "install"
                environment = os.environ.copy()
                environment.update({"PATH": str(tools) + os.pathsep + environment["PATH"],
                                    "DEB_BUILD_OPTIONS": "parallel=2", "DESTDIR": str(destination),
                                    "QPID_TEST_TRACE": str(trace), "QPID_TEST_OUTPUTS": json.dumps(OUTPUTS)})
                helper = ROOT / "packaging/ubuntu" / suite / "v8-stable/debian/build-qpid-proton.sh"
                result = subprocess.run(["bash", str(helper)], cwd=str(work), env=environment,
                                        stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                        universal_newlines=True, timeout=30)
                self.assertEqual(result.returncode, 0, result.stdout)
                events = [json.loads(line) for line in trace.read_text().splitlines()]
                self.assertEqual(events[0]["phase"], "configure")
                consumers = [event for event in events if event["phase"] == "parallel-consumers"]
                self.assertEqual(len(consumers), 1)
                self.assertIn("-j2", consumers[0]["jobs"])
                self.assertEqual(events[-1]["phase"], "install")
                self.assertEqual((destination / "usr/lib/qpid-fixture").read_text(), "installed\n")
                self.assertTrue(consumers[0]["complete"], "parallel consumers started before outputs were complete")


if __name__ == "__main__":
    unittest.main()
