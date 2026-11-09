#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""TEST ONLY: replay original Debian rsyslog8.2610.0-3 buildinfo.

Preserves package sources, Debian patches, recorded dependencies and Build-Path
via debrebuild. CPU/kernel/network and outer bootstrap tools still differ.
HTTPS download hashes are pinned; GPG-signature verification is not claimed.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import signal
import subprocess
import sys
import time
import urllib.request

INPUTS = {
    "amd64": "7351544c731542e9c395ff58af8b7bab1fcbd02886d06cfcaddce529f3d26a7f",
    "arm64": "79f9a856bb58f4913e41fcc4b615211e49b6152d19b304ddb30c4bea6b0ac268",
    "armhf": "af5ae6b2b00b5845a37a2ee65ab4ead21303eda08bc52d99f8674a94e8f857a7",
}
BASE = "https://buildinfos.debian.net/buildinfo-pool/r/rsyslog/"
ROOT = Path(__file__).resolve().parents[1]


def phase(operation, rc=None):
    cmd = [str(ROOT / "devtools/ci-flake-phase.sh"), operation, "debian-rebuild", "automake"]
    subprocess.run(cmd + ([] if rc is None else [str(rc)]), check=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--arch", choices=INPUTS, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--download-only", action="store_true")
    parser.add_argument("--parallelism", type=int, help="explicit lower-fidelity concurrency override")
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    name = "rsyslog_8.2610.0-3_" + args.arch + ".buildinfo"
    with urllib.request.urlopen(BASE + name, timeout=60) as response:
        data = response.read()
    actual = hashlib.sha256(data).hexdigest()
    if actual != INPUTS[args.arch]:
        raise RuntimeError("buildinfo SHA256 mismatch: " + actual)
    text = data.decode("utf-8")
    if "Version: 8.2610.0-3\n" not in text or "Build-Architecture: " + args.arch + "\n" not in text:
        raise RuntimeError("unexpected source version or build architecture")
    buildinfo = args.output / name
    buildinfo.write_bytes(data)
    metadata = {"source_version": "8.2610.0-3", "architecture": args.arch,
                "buildinfo_url": BASE + name, "buildinfo_sha256": actual,
                "signature_verified": False, "source": "Debian package, not PR source",
                "harness_commit": os.environ.get("GITHUB_SHA", "local-unpublished-harness"),
                "recorded_parallelism": re.search(r'DEB_BUILD_OPTIONS="([^"]+)"', text).group(1),
                "limits": ["CPU/kernel/network and outer bootstrap tools differ from Debian builder."]}
    if args.parallelism is not None:
        if not 1 <= args.parallelism <= 64:
            parser.error("--parallelism must be between1 and64")
        # Strip the original clear-signature for a visibly derived input. Keep
        # the pinned signed input unchanged; do not suggest its signature covers
        # the modified build environment.
        unsigned = text.split("\n\n", 1)[1].split("\n-----BEGIN PGP SIGNATURE-----", 1)[0]
        unsigned, replaced = re.subn(r'( DEB_BUILD_OPTIONS="[^"\n]*?)parallel=\d+',
                                     lambda match: match[1] + "parallel=" + str(args.parallelism), unsigned)
        if replaced != 1:
            raise RuntimeError("could not uniquely override recorded parallelism")
        buildinfo = args.output / ("derived-parallel" + str(args.parallelism) + "-" + name)
        buildinfo.write_text(unsigned.rstrip() + "\n")
        metadata.update(parallelism_override=args.parallelism, derived_unsigned_input=True,
                        effective_buildinfo_sha256=hashlib.sha256(buildinfo.read_bytes()).hexdigest())
        metadata["limits"].append("Recorded concurrency explicitly overridden; not an exact environment replay.")
    (args.output / "rebuild.json").write_text(json.dumps(metadata, indent=2) + "\n")
    if args.download_only:
        print(json.dumps(metadata))
        return 0
    phase_started = False
    process = None
    try:
        native = subprocess.check_output(["dpkg", "--print-architecture"], text=True).strip()
        if native != args.arch:
            raise RuntimeError("native architecture " + native + " does not match " + args.arch)
        # Do not change kernel/AppArmor policy if the runner cannot use this backend.
        subprocess.run(["unshare", "--user", "--map-root-user", "true"], check=True)
        command = ["debrebuild", "--buildresult=" + str(args.output / "packages"),
                   "--builder=sbuild+unshare", "--cache=" + str(args.output / "cache"), "--", str(buildinfo)]
        metadata["command"] = command
        started = time.monotonic()
        phase_started = False
        counts = {}
        summary_fields = set()
        summary_complete = False
        with (args.output / "rebuild.log").open("w") as log:
            process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                       text=True, errors="replace", bufsize=1, start_new_session=True)
            for line in process.stdout:
                sys.stdout.write(line)
                sys.stdout.flush()
                log.write(line)
                log.flush()
                # Every newly observed test invalidates an earlier (possibly
                # nested) summary. Only a fresh complete summary after the last
                # test can establish that all observed test work completed.
                test_line = (re.search(r"Test: \./", line) or
                             re.match(r"(?:PASS|FAIL|SKIP|ERROR|XFAIL|XPASS): [A-Za-z0-9_.+/-]+\s*$", line))
                if test_line:
                    summary_fields.clear()
                    summary_complete = False
                    if not phase_started:
                        phase("begin")
                        phase_started = True
                match = re.match(r"#\s*(TOTAL|PASS|SKIP|FAIL|ERROR|XFAIL|XPASS):\s*(\d+)", line)
                if match:
                    if match.group(1) == "TOTAL":
                        summary_fields.clear()
                        summary_complete = False
                    counts[match.group(1)] = int(match.group(2))
                    summary_fields.add(match.group(1))
                    summary_complete = {"TOTAL", "FAIL", "ERROR"}.issubset(summary_fields)
            rc = process.wait()
        if phase_started:
            # Successful tests followed by a package/diff failure are not flakes.
            test_rc = int(bool(counts.get("FAIL", 0) or counts.get("ERROR", 0)
                          or counts.get("XPASS", 0) or not summary_complete))
            if not summary_complete:
                rc = rc or 2
            phase("end", test_rc)
            evidence = Path(os.environ.get("RSYSLOG_FLAKE_EVIDENCE_ROOT", ".ci/flake-evidence"))
            target = evidence / "logs/debian-rebuild.log"
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes((args.output / "rebuild.log").read_bytes())
        metadata.update(exit_code=rc, test_counts=counts, test_phase_started=phase_started,
                        test_summary_complete=summary_complete,
                        seconds=round(time.monotonic() - started, 2))
        metadata_text = json.dumps(metadata, indent=2) + "\n"
        (args.output / "rebuild.json").write_text(metadata_text)
        if phase_started:
            # The normal collector downloads only ci-failure-* artifacts. Keep
            # actual Debian source identity beside the log, not only in the
            # separate experiment artifact whose GitHub SHA names the harness.
            (evidence / "logs/debian-rebuild-provenance.log").write_text(metadata_text)
        return rc
    except (OSError, RuntimeError, subprocess.SubprocessError) as error:
        # Popen owns a new session, so only this replay's process group is
        # stopped. Do not leave sbuild descendants running after logging fails.
        if process is not None and process.poll() is None:
            try:
                os.killpg(process.pid, signal.SIGTERM)
                try:
                    process.wait(timeout=10)
                except subprocess.TimeoutExpired:
                    os.killpg(process.pid, signal.SIGKILL)
                    process.wait(timeout=10)
            except ProcessLookupError:
                pass
        if phase_started:
            try:
                phase("end", 1)
            except subprocess.SubprocessError as phase_error:
                print("could not finalize interrupted phase: " + str(phase_error), file=sys.stderr)
        metadata.update(exit_code=2, test_phase_started=phase_started)
        metadata["execution_error" if phase_started else "setup_error"] = str(error)
        metadata_text = json.dumps(metadata, indent=2) + "\n"
        (args.output / "rebuild.json").write_text(metadata_text)
        if phase_started:
            evidence = Path(os.environ.get("RSYSLOG_FLAKE_EVIDENCE_ROOT", ".ci/flake-evidence"))
            logs = evidence / "logs"
            logs.mkdir(parents=True, exist_ok=True)
            (logs / "debian-rebuild.log").write_bytes((args.output / "rebuild.log").read_bytes())
            (logs / "debian-rebuild-provenance.log").write_text(metadata_text)
        print("rebuild setup/execution failed: " + str(error), file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
