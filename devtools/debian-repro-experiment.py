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
        with (args.output / "rebuild.log").open("w") as log:
            process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                       text=True, bufsize=1)
            for line in process.stdout:
                sys.stdout.write(line)
                sys.stdout.flush()
                log.write(line)
                log.flush()
                # Actual test headers/results, never dependency/configure output.
                if not phase_started and (re.search(r"Test: \./", line) or
                                          re.match(r"(?:PASS|FAIL|SKIP|ERROR): .*\.sh", line)):
                    phase("begin")
                    phase_started = True
                match = re.match(r"#\s*(TOTAL|PASS|SKIP|FAIL|ERROR):\s*(\d+)", line)
                if match:
                    counts[match.group(1)] = int(match.group(2))
            rc = process.wait()
        if phase_started:
            # Successful tests followed by a package/diff failure are not flakes.
            test_rc = int(bool(counts.get("FAIL", 0) or counts.get("ERROR", 0) or "TOTAL" not in counts))
            phase("end", test_rc)
            evidence = Path(os.environ.get("RSYSLOG_FLAKE_EVIDENCE_ROOT", ".ci/flake-evidence"))
            target = evidence / "logs/debian-rebuild.log"
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes((args.output / "rebuild.log").read_bytes())
        metadata.update(exit_code=rc, test_counts=counts, test_phase_started=phase_started,
                        seconds=round(time.monotonic() - started, 2))
        (args.output / "rebuild.json").write_text(json.dumps(metadata, indent=2) + "\n")
        return rc
    except (OSError, RuntimeError, subprocess.SubprocessError) as error:
        metadata.update(exit_code=2, setup_error=str(error), test_phase_started=phase_started)
        (args.output / "rebuild.json").write_text(json.dumps(metadata, indent=2) + "\n")
        print("rebuild setup/execution failed: " + str(error), file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
