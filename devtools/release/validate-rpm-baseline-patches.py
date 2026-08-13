#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 Rainer Gerhards and Others
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
"""Verify and remove reviewed downstream RPM baseline patches.

Daily-stable packages replace a distribution's source archive with current
upstream.  Every downstream patch must therefore be explicitly reviewed,
content-pinned, and removed before the RPM spec is reused.
"""

import hashlib
import json
import pathlib
import re
import sys


def fail(message):
    raise SystemExit(message)


def main():
    if len(sys.argv) != 4:
        fail("usage: validate-rpm-baseline-patches.py <spec> <policy> <distro>")

    spec_path = pathlib.Path(sys.argv[1])
    policy_path = pathlib.Path(sys.argv[2])
    distro = sys.argv[3]
    spec = spec_path.read_text(encoding="utf-8")
    policy = json.loads(policy_path.read_text(encoding="utf-8"))

    if "allowed_patch_skips" in policy:
        fail(f"{distro} policy must use integrated_baseline_patches")
    reviewed = policy.get("integrated_baseline_patches", [])
    if not isinstance(reviewed, list):
        fail("integrated_baseline_patches must be a list")

    reviewed_hashes = {}
    for entry in reviewed:
        if not isinstance(entry, dict):
            fail("integrated baseline patch entry must be an object")
        name = entry.get("name", "").strip()
        digest = entry.get("sha256", "").lower()
        reason = entry.get("reason", "").strip()
        if not name or not re.fullmatch(r"[A-Za-z0-9._+-]+", name):
            fail("integrated baseline patch has an invalid name")
        if not re.fullmatch(r"[0-9a-f]{64}", digest):
            fail(f"integrated baseline patch {name} has an invalid SHA-256")
        if not reason:
            fail(f"integrated baseline patch {name} has no reason")
        if name in reviewed_hashes:
            fail(f"integrated baseline patch {name} is listed more than once")
        reviewed_hashes[name] = digest

    declarations = re.findall(r"(?m)^Patch(\d*):\s*(\S+)\s*$", spec)
    numbers = [number for number, _ in declarations]
    names = [name for _, name in declarations]
    normalized_numbers = [int(number or "0") for number in numbers]
    duplicate_numbers = sorted(
        str(number)
        for number in set(normalized_numbers)
        if normalized_numbers.count(number) > 1
    )
    duplicate_names = sorted(name for name in set(names) if names.count(name) > 1)
    if duplicate_numbers or duplicate_names:
        details = []
        if duplicate_numbers:
            details.append("numbers: " + ", ".join(duplicate_numbers))
        if duplicate_names:
            details.append("names: " + ", ".join(duplicate_names))
        fail(f"duplicate {distro} baseline patch declaration (" + "; ".join(details) + ")")

    baseline_names = set(names)
    unreviewed = sorted(baseline_names - set(reviewed_hashes))
    missing = sorted(set(reviewed_hashes) - baseline_names)
    if unreviewed:
        fail(
            f"{distro} baseline gained unreviewed patches; review them before packaging main: "
            + ", ".join(unreviewed)
        )
    if missing:
        fail(
            f"reviewed baseline patches are absent from {distro} baseline: "
            + ", ".join(missing)
        )

    sources_dir = spec_path.parent.parent / "SOURCES"
    patch_paths = []
    for number, name in declarations:
        patch_path = sources_dir / name
        if not patch_path.is_file():
            fail(f"reviewed baseline patch is missing: {name}")
        digest = hashlib.sha256(patch_path.read_bytes()).hexdigest()
        if digest != reviewed_hashes[name]:
            fail(f"reviewed baseline patch changed: {name}")
        patch_paths.append(patch_path)

        spec, declaration_count = re.subn(
            rf"(?m)^Patch{re.escape(number)}:\s*{re.escape(name)}\s*\n", "", spec
        )
        if number:
            application_pattern = (
                rf"(?m)^%patch(?:{re.escape(number)}(?=\s|$)|"
                rf"\s+-P\s*{re.escape(number)}|\s+-P{re.escape(number)}|"
                rf"\s+{re.escape(number)})(?:\s+[^\n]*)?\s*\n"
            )
        else:
            application_pattern = (
                r"(?m)^%patch(?:0(?=\s|$)|\s+-P\s*0(?=\s|$)|"
                r"\s+-P0(?=\s|$)|\s+0(?=\s|$)|"
                r"(?!\s+(?:-P\s*[1-9]\d*|-P[1-9]\d*|[1-9]\d*)(?:\s|$)))"
                r"(?:\s+[^\n]*)?\s*\n"
            )
        spec, application_count = re.subn(application_pattern, "", spec)
        if declaration_count != 1:
            fail(f"could not remove exactly one declaration for {name}")
        if application_count == 0 and not re.search(r"(?m)^%autosetup(?:\s|$)", spec):
            fail(f"could not find an application for {name}")
        if application_count > 1:
            fail(f"could not remove exactly one application for {name}")

    spec_path.write_text(spec, encoding="utf-8")
    for patch_path in patch_paths:
        patch_path.unlink()


if __name__ == "__main__":
    main()
