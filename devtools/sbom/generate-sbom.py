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
"""Write a CycloneDX 1.6 SBOM for an rsyslog source release.

The SBOM describes the source tarball: rsyslog itself, code copied into the
tree (vendored), and the external libraries the build system can link.  The
libraries are found in configure.ac and the non-test Makefile.am files and
mapped to components through devtools/sbom/sbom-components.toml.

Only the Python standard library is used (Python 3.11 or later).
See devtools/sbom/README.md.
"""

import argparse
import datetime
import hashlib
import json
import os
import re
import subprocess
import sys
import tomllib
import uuid

TOOL_NAME = "rsyslog-generate-sbom"
TOOL_VERSION = "1"
SPEC_VERSION = "1.6"
PROP_PREFIX = "rsyslog:sbom:"

SCAN_MACROS = ("PKG_CHECK_MODULES", "AC_CHECK_LIB", "AC_SEARCH_LIBS")
# -l<name> is only taken from lines that set linker flags.
LINK_LINE_RE = re.compile(r"(LIBS|LIBADD|LDADD|LDFLAGS)\b")
LINK_FLAG_RE = re.compile(r"(?<![\w$-])-l(:?[A-Za-z][\w.+-]*)")
PKGCONFIG_CALL_RE = re.compile(
    r"(?:\$PKG_CONFIG|\$\{PKG_CONFIG\}|pkg-config)\s+--(?:libs|cflags|exists)\s+([A-Za-z][\w.+-]*)")
AC_INIT_RE = re.compile(r"AC_INIT\(\s*\[?([^\],]+)\]?\s*,\s*\[?([^\],]+)\]?")
VERSION_OP_RE = re.compile(r"^(>=|<=|=|>|<)$")
VENDOR_README_ROW_RE = re.compile(r"^\|\s*([^|]+?)\s*\|\s*([0-9][^|]*?)\s*\|")


class SbomError(Exception):
    pass


def link_name(flag_value):
    """Name for a -l argument; GNU ld -l:libfoo-static.a becomes 'foo'."""
    if not flag_value.startswith(":"):
        return flag_value
    name = flag_value[1:]
    name = re.sub(r"\.(a|so(\.[0-9.]+)?)$", "", name)
    if name.startswith("lib"):
        name = name[3:]
    if name.endswith("-static"):
        name = name[:-len("-static")]
    return name


def strip_comments(text):
    """Blank out shell (#) and m4 (dnl) comment lines, keeping line numbers."""
    out = []
    for line in text.splitlines():
        s = line.lstrip()
        out.append("" if s.startswith("#") or s.startswith("dnl") else line)
    return "\n".join(out)


def macro_args(text, open_paren):
    """Split the arguments of an m4 macro call starting at '('.

    m4 quotes ([ ]) protect commas and parentheses.  Returns the list of
    top-level arguments with quotes and whitespace removed.
    """
    depth = 0
    quote = 0
    args = []
    cur = []
    for i in range(open_paren, len(text)):
        c = text[i]
        if c == "[":
            quote += 1
        elif c == "]":
            quote -= 1
        if quote == 0 and c == "(":
            depth += 1
            if depth == 1:
                continue
        elif quote == 0 and c == ")":
            depth -= 1
            if depth == 0:
                args.append("".join(cur))
                break
        if c == "," and depth == 1 and quote == 0:
            args.append("".join(cur))
            cur = []
        else:
            cur.append(c)
    return [a.strip().strip("[]").strip() for a in args]


def parse_module_list(spec):
    """Parse a pkg-config module list such as 'libevent >= 2.0, libevent_pthreads'.

    Returns a list of (module, constraint-or-None).
    """
    tokens = spec.replace(",", " ").split()
    result = []
    i = 0
    while i < len(tokens):
        name = tokens[i]
        constraint = None
        if i + 2 < len(tokens) and VERSION_OP_RE.match(tokens[i + 1]):
            constraint = f"{tokens[i + 1]} {tokens[i + 2]}"
            i += 2
        if not name.startswith("$"):
            result.append((name, constraint))
        i += 1
    return result


def scan_configure_ac(text, origin):
    """Return {name: {"constraints": set, "sources": set}} found in configure.ac."""
    found = {}

    def add(name, origin_line, constraint=None):
        entry = found.setdefault(name, {"constraints": set(), "sources": set()})
        entry["sources"].add(origin_line)
        if constraint:
            entry["constraints"].add(constraint)

    clean = strip_comments(text)
    macro_re = re.compile(r"\b(" + "|".join(SCAN_MACROS) + r")\s*\(")
    for m in macro_re.finditer(clean):
        args = macro_args(clean, m.end() - 1)
        line = clean.count("\n", 0, m.start()) + 1
        where = f"{origin}:{line}"
        if m.group(1) == "PKG_CHECK_MODULES":
            if len(args) >= 2:
                for name, constraint in parse_module_list(args[1]):
                    add(name, where, constraint)
        elif m.group(1) == "AC_CHECK_LIB":
            if args and args[0] and not args[0].startswith("$"):
                add(args[0], where)
        else:  # AC_SEARCH_LIBS(function, libs...)
            if len(args) >= 2:
                for name in args[1].split():
                    if not name.startswith("$"):
                        add(name, where)
    for lineno, line in enumerate(clean.splitlines(), 1):
        where = f"{origin}:{lineno}"
        for m in PKGCONFIG_CALL_RE.finditer(line):
            add(link_name(m.group(1)), where)
        if LINK_LINE_RE.search(line):
            for m in LINK_FLAG_RE.finditer(line):
                add(link_name(m.group(1)), where)
    return found


def scan_makefile_am(text, origin, found):
    clean = strip_comments(text)
    for lineno, line in enumerate(clean.splitlines(), 1):
        if LINK_LINE_RE.search(line):
            for m in LINK_FLAG_RE.finditer(line):
                entry = found.setdefault(link_name(m.group(1)), {"constraints": set(), "sources": set()})
                entry["sources"].add(f"{origin}:{lineno}")


def list_makefile_ams(srcdir):
    """Non-test Makefile.am files, from git when available."""
    try:
        out = subprocess.run(["git", "-C", srcdir, "ls-files", "*Makefile.am"],
                             check=True, capture_output=True, text=True).stdout
        files = [f for f in out.splitlines() if f]
    except (OSError, subprocess.CalledProcessError):
        files = []
        for root, dirs, names in os.walk(srcdir):
            dirs[:] = [d for d in dirs if not d.startswith(".")]
            if "Makefile.am" in names:
                files.append(os.path.relpath(os.path.join(root, "Makefile.am"), srcdir))
    return sorted(f for f in files if not f.startswith("tests/"))


def scan_tree(srcdir):
    with open(os.path.join(srcdir, "configure.ac"), encoding="utf-8") as fh:
        configure_ac = fh.read()
    found = scan_configure_ac(configure_ac, "configure.ac")
    for rel in list_makefile_ams(srcdir):
        with open(os.path.join(srcdir, rel), encoding="utf-8") as fh:
            scan_makefile_am(fh.read(), rel, found)
    return configure_ac, found


def read_ac_init_version(configure_ac):
    m = AC_INIT_RE.search(configure_ac)
    if not m:
        raise SbomError("cannot find AC_INIT version in configure.ac")
    return m.group(2).strip()


def resolve(found, manifest):
    """Map scanned names to manifest dependencies.

    Returns (resolved, unknown) where resolved is {dep_name: {"constraints"}}
    and unknown is {scanned_name: sources}.
    """
    ignore = set(manifest.get("ignore", {}).get("names", []))
    by_match = {}
    for dep in manifest.get("dependency", []):
        for alias in dep.get("match", []):
            if alias in by_match:
                raise SbomError(f"manifest: '{alias}' is matched by both "
                                f"{by_match[alias]['name']} and {dep['name']}")
            by_match[alias] = dep
    resolved = {}
    unknown = {}
    for name, info in found.items():
        if name in ignore:
            continue
        dep = by_match.get(name)
        if dep is None:
            unknown[name] = info["sources"]
            continue
        entry = resolved.setdefault(dep["name"], {"constraints": set()})
        entry["constraints"].update(info["constraints"])
    for dep in manifest.get("dependency", []):
        if dep.get("declared"):
            resolved.setdefault(dep["name"], {"constraints": set()})
    return resolved, unknown


def check_manifest(srcdir, manifest, found):
    """Return a list of problems that make the manifest stale."""
    problems = []
    _, unknown = resolve(found, manifest)
    for name in sorted(unknown):
        where = ", ".join(sorted(unknown[name])[:3])
        problems.append(f"library '{name}' ({where}) is not in sbom-components.toml: "
                        "add it to a [[dependency]] 'match' list or to [ignore]")
    for dep in manifest.get("dependency", []):
        if not dep.get("declared") and not any(a in found for a in dep.get("match", [])):
            problems.append(f"dependency '{dep['name']}' is no longer used by the build system: remove it")
    for comp in manifest.get("vendored", []):
        for path in comp.get("paths", []):
            if not os.path.exists(os.path.join(srcdir, path)):
                problems.append(f"vendored '{comp['name']}': path '{path}' does not exist")
    readme = os.path.join(srcdir, "doc/source/_static/vendor/README.md")
    if os.path.exists(readme):
        versions = {}
        with open(readme, encoding="utf-8") as fh:
            for line in fh:
                m = VENDOR_README_ROW_RE.match(line)
                if m:
                    versions[m.group(1)] = m.group(2)
        for comp in manifest.get("vendored", []):
            if comp["name"] in versions and versions[comp["name"]] != comp.get("version"):
                problems.append(f"vendored '{comp['name']}': version {comp.get('version')} differs "
                                f"from doc/source/_static/vendor/README.md ({versions[comp['name']]})")
    return problems


def sha256_file(path):
    h = hashlib.sha256()
    with open(path, "rb") as fh:
        for chunk in iter(lambda: fh.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def timestamp():
    epoch = os.environ.get("SOURCE_DATE_EPOCH")
    when = (datetime.datetime.fromtimestamp(int(epoch), datetime.timezone.utc) if epoch
            else datetime.datetime.now(datetime.timezone.utc))
    return when.replace(microsecond=0).isoformat().replace("+00:00", "Z")


def ext_refs(item):
    refs = []
    if item.get("website"):
        refs.append({"type": "website", "url": item["website"]})
    return refs


def prop(name, value):
    return {"name": PROP_PREFIX + name, "value": value}


def build_bom(manifest, version, resolved, tarball=None, release_url=None):
    project = manifest["project"]
    tag = project.get("tag_prefix", "") + version
    main_purl = f"{project['purl']}@{tag}"
    tar_hash = sha256_file(tarball) if tarball else None

    main = {
        "type": "application",
        "bom-ref": main_purl,
        "supplier": {"name": project["supplier"], "url": [project["website"]]},
        "name": project["name"],
        "version": version,
        "licenses": [{"expression": project["license_expression"]}],
        "purl": main_purl,
        "externalReferences": [
            {"type": "website", "url": project["website"]},
            {"type": "vcs", "url": project["vcs"]},
            {"type": "advisories", "url": project["advisories"]},
        ],
    }
    if tarball:
        main["hashes"] = [{"alg": "SHA-256", "content": tar_hash}]
        dist = {"type": "distribution", "hashes": [{"alg": "SHA-256", "content": tar_hash}],
                "comment": os.path.basename(tarball)}
        dist["url"] = release_url or f"{project['vcs']}/releases/tag/{tag}"
        main["externalReferences"].append(dist)

    components = []
    for comp in sorted(manifest.get("vendored", []), key=lambda c: c["name"]):
        c = {
            "type": "library",
            "bom-ref": f"vendored:{comp['name']}",
            "name": comp["name"],
            "scope": "required",
            "description": comp.get("description", ""),
            "licenses": [{"license": ({"id": comp["license"]} if comp.get("license")
                                      else {"name": comp.get("license_name", "NOASSERTION")})}],
            "properties": [prop("origin", "vendored")],
            "evidence": {"occurrences": [{"location": p} for p in comp["paths"]]},
        }
        if comp.get("version"):
            c["version"] = comp["version"]
        if comp.get("supplier"):
            c["supplier"] = {"name": comp["supplier"]}
        if comp.get("purl"):
            c["purl"] = comp["purl"]
        if ext_refs(comp):
            c["externalReferences"] = ext_refs(comp)
        components.append(c)

    deps = {d["name"]: d for d in manifest.get("dependency", [])}
    for name in sorted(resolved):
        dep = deps.get(name, {"name": name})
        c = {
            "type": "library",
            "bom-ref": f"external:{name}",
            "name": name,
            "scope": "required" if dep.get("required") else "optional",
            "description": dep.get("description",
                                   "External library; the version is the one installed on the build system."),
            "properties": [prop("origin", "external")],
        }
        if dep.get("purl"):
            c["purl"] = dep["purl"]
        for constraint in sorted(resolved[name]["constraints"]):
            c["properties"].append(prop("version-constraint", constraint))
        if ext_refs(dep):
            c["externalReferences"] = ext_refs(dep)
        components.append(c)

    for c in components:
        if not c.get("description"):
            del c["description"]

    serial_seed = f"{main_purl}#{tar_hash or 'no-tarball'}"
    return {
        "bomFormat": "CycloneDX",
        "specVersion": SPEC_VERSION,
        "serialNumber": f"urn:uuid:{uuid.uuid5(uuid.NAMESPACE_URL, serial_seed)}",
        "version": 1,
        "metadata": {
            "timestamp": timestamp(),
            "lifecycles": [{"phase": "pre-build"}],
            "tools": {"components": [{"type": "application", "name": TOOL_NAME,
                                      "version": TOOL_VERSION}]},
            "component": main,
            "supplier": {"name": project["supplier"], "url": [project["website"]]},
        },
        "components": components,
        "dependencies": [
            {"ref": main_purl, "dependsOn": [c["bom-ref"] for c in components]},
        ] + [{"ref": c["bom-ref"], "dependsOn": []} for c in components],
    }


def load_manifest(path):
    with open(path, "rb") as fh:
        manifest = tomllib.load(fh)
    if "project" not in manifest:
        raise SbomError(f"{path}: missing [project] table")
    return manifest


def main(argv=None):
    here = os.path.dirname(os.path.abspath(__file__))
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--srcdir", default=os.path.normpath(os.path.join(here, "..", "..")),
                    help="source tree with configure.ac (default: this checkout)")
    ap.add_argument("--manifest", default=os.path.join(here, "sbom-components.toml"))
    ap.add_argument("--version", help="release version (default: AC_INIT in configure.ac)")
    ap.add_argument("--tarball", help="release tarball to hash into the SBOM")
    ap.add_argument("--release-url", help="download URL of the tarball")
    ap.add_argument("-o", "--output", help="output file (default: stdout)")
    ap.add_argument("--check", action="store_true",
                    help="only verify that the manifest matches the build system")
    args = ap.parse_args(argv)

    try:
        manifest = load_manifest(args.manifest)
        configure_ac, found = scan_tree(args.srcdir)
        problems = check_manifest(args.srcdir, manifest, found)
        if args.check:
            for p in problems:
                print(f"ERROR: {p}", file=sys.stderr)
            if problems:
                return 1
            print(f"sbom manifest OK ({len(found)} library names scanned)")
            return 0
        for p in problems:
            print(f"WARNING: {p}", file=sys.stderr)
        _, unknown = resolve(found, manifest)
        resolved, _ = resolve({k: v for k, v in found.items() if k not in unknown}, manifest)
        for name in unknown:  # keep unknown libraries visible in the SBOM
            resolved[name] = {"constraints": found[name]["constraints"]}
        version = args.version or read_ac_init_version(configure_ac)
        bom = build_bom(manifest, version, resolved, args.tarball, args.release_url)
    except (OSError, SbomError, tomllib.TOMLDecodeError) as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 1

    text = json.dumps(bom, indent=2, sort_keys=False) + "\n"
    if args.output:
        with open(args.output, "w", encoding="utf-8") as fh:
            fh.write(text)
    else:
        sys.stdout.write(text)
    return 0


if __name__ == "__main__":
    sys.exit(main())
