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
"""Require daily-stable policy for every auto-enabled optional module.

A configure option whose default is ``optional`` builds its module whenever
the dependencies happen to be present.  Native RPM and Debian baselines list
their files explicitly, so such a module breaks every daily-stable target the
first night after it lands on main.  This check runs in the regular testbench
and fails the pull request that adds the module instead: each one must be a
shared package in the feature contract, or be handled by every RPM and
Debian target policy.

Alpine is not checked: its APKBUILD packages modules generically.

usage: check-optional-module-policy.py [<repo-root>]
"""

import json
import pathlib
import re
import sys

RPM_POLICIES = [
    ".github/fedora44-daily-stable-policy.yml",
    ".github/el10-daily-stable-policy.yml",
    ".github/amazonlinux2023-daily-stable-policy.yml",
    ".github/opensuse-leap160-daily-stable-policy.yml",
]
DEB_POLICIES = [
    ".github/debian-daily-stable-13-policy.yml",
    ".github/ubuntu-daily-stable-26.04-policy.yml",
]
FEATURE_CONTRACT = ".github/daily-stable-package-features.yml"


def auto_enabled_modules(configure_ac):
    blocks = re.split(r"(?=AC_ARG_ENABLE\()", configure_ac)
    modules = []
    for block in blocks:
        match = re.match(r"AC_ARG_ENABLE\(\s*\[?([A-Za-z0-9_-]+)\]?\s*,", block)
        if not match:
            continue
        name = match.group(1)
        variable = "enable_" + name.replace("-", "_")
        if re.search(rf"\[\s*{re.escape(variable)}=\"?optional\"?\s*\]\s*\)", block):
            modules.append(name)
    return modules


def main():
    root = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else ".")
    modules = auto_enabled_modules((root / "configure.ac").read_text(encoding="utf-8"))
    contract = json.loads((root / FEATURE_CONTRACT).read_text(encoding="utf-8"))
    packaged = {entry.get("module") for entry in contract.get("module_packages", [])}

    problems = []
    for module in modules:
        if module in packaged:
            continue
        for path in RPM_POLICIES:
            policy = json.loads((root / path).read_text(encoding="utf-8"))
            disabled = {e.get("module") for e in policy.get("disabled_optional_modules", [])}
            if module not in disabled:
                problems.append(f"{path}: add {module} to disabled_optional_modules")
        for path in DEB_POLICIES:
            policy = json.loads((root / path).read_text(encoding="utf-8"))
            paths = [e.get("path", "") for e in policy.get("not_installed_paths", [])]
            if not any(p.endswith(f"/{module}.so") for p in paths):
                problems.append(f"{path}: add usr/lib/*/rsyslog/{module}.so to not_installed_paths")

    if problems:
        print(
            "auto-enabled optional modules need daily-stable package policy, "
            "or native package builds fail on unpackaged files:",
            file=sys.stderr,
        )
        for problem in problems:
            print(f"  {problem}", file=sys.stderr)
        return 1
    print("auto-enabled optional modules covered: " + (", ".join(modules) or "none"))
    return 0


if __name__ == "__main__":
    sys.exit(main())
