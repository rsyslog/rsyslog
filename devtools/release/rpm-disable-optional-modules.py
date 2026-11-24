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
"""Keep auto-enabled optional modules out of a native RPM baseline.

Current upstream builds some optional modules automatically when their
dependencies are present.  A native spec that predates such a module has no
package for it, so rpmbuild stops on unpackaged files.  The policy key
``disabled_optional_modules`` lists those modules; each is disabled right
after the spec's ``%configure`` line.  A native spec that explicitly enables
a listed module (any form other than ``--enable-MODULE=no``) is rejected so
its packaging gets reviewed instead of being silently overridden.
"""

import json
import pathlib
import re
import sys


def main():
    if len(sys.argv) != 4:
        raise SystemExit("usage: rpm-disable-optional-modules.py <spec> <policy> <distro>")
    spec_path = pathlib.Path(sys.argv[1])
    policy = json.loads(pathlib.Path(sys.argv[2]).read_text(encoding="utf-8"))
    distro = sys.argv[3]
    spec = spec_path.read_text(encoding="utf-8")

    for entry in policy.get("disabled_optional_modules", []):
        module = entry.get("module", "")
        if not re.fullmatch(r"[a-z][a-z0-9]*", module) or not entry.get("reason", "").strip():
            raise SystemExit("disabled optional module requires a valid module and reason")
        if re.search(rf"--enable-{re.escape(module)}(?:\s|$|=(?!no(?:\s|$)))", spec):
            raise SystemExit(f"native {distro} spec explicitly enables optional module: {module}")
        flag = f"--disable-{module}"
        if flag in spec:
            continue
        spec, count = re.subn(
            r"(?m)^(%configure[ \t]*\\)$",
            lambda match: match.group(1) + f"\n\t{flag} \\",
            spec,
            count=1,
        )
        if count != 1:
            raise SystemExit(f"could not disable optional module in {distro} spec: {module}")

    spec_path.write_text(spec, encoding="utf-8")


if __name__ == "__main__":
    main()
