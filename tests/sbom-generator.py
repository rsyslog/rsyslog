#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
#
# Copyright 2026 Rainer Gerhards and Adiscon GmbH.
"""Unit tests for devtools/sbom/generate-sbom.py.

What is tested and why:
- Scanning: every way configure.ac and Makefile.am name a library
  (PKG_CHECK_MODULES incl. version constraints and multi-line calls,
  AC_CHECK_LIB, AC_SEARCH_LIBS, pkg-config calls, -l<name> and GNU ld
  -l:<file>) must be found, and comments, non-linker "-l" text and
  tests/Makefile.am must not be.  A missed library would silently drop
  out of the release SBOM.
- Drift detection: --check must fail on an unknown library, an unused
  manifest entry, a missing vendored path and a vendored version that
  differs from doc/source/_static/vendor/README.md, and must exit 0 on a
  consistent tree.  This is what keeps the manifest complete over time.
- Output: the CycloneDX document must carry the required top-level
  fields, scopes, version constraints, vendored file locations and the
  tarball hash; every dependency ref must point at a component; output
  must be byte-identical for the same input and SOURCE_DATE_EPOCH.
- Configure options: each optional library must name the configure
  options under which the build looks for it (shell if/elif, AS_IF,
  AC_ARG_ENABLE actions, AC_DEFUN bodies via their call sites, and
  plugins/<module>/Makefile.am via --enable-<module>).  Else branches must
  not be attributed, quoted C code must not break m4 quote tracking, a
  probe outside any option must be flagged, and required libraries carry
  no options.  An unknown "configure_options" entry must fail --check.
  Without this a distributor cannot tell which SBOM entries apply to a
  given build configuration.
- Real tree: --check on this source tree must pass, so a PR that adds a
  library without updating the manifest fails here and in CI.

Oracle: unittest assertions on the parsed data and on the exit status
and output of the command-line tool.  Exit status 0 means all passed.
"""

import contextlib
import hashlib
import importlib.util
import io
import json
import os
import subprocess
import sys
import tempfile
import textwrap
import unittest

TESTS_DIR = os.path.dirname(os.path.abspath(__file__))
SRCDIR = os.path.normpath(os.path.join(TESTS_DIR, ".."))
GENERATOR = os.path.join(SRCDIR, "devtools", "sbom", "generate-sbom.py")


def load_generator():
    spec = importlib.util.spec_from_file_location("generate_sbom", GENERATOR)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


gen = load_generator()

CONFIGURE_AC = textwrap.dedent("""\
    AC_INIT([demo],[1.2.3],[demo@example.org])
    PKG_CHECK_MODULES(LIBESTR, libestr >= 0.1.9)
    PKG_CHECK_MODULES([EVENT], [libevent >= 2.0, libevent_pthreads])
    PKG_CHECK_MODULES(
            [CAPNG],
            [libcap-ng >= 0.8.2]
    )
    # PKG_CHECK_MODULES(COMMENTED, commented-out-lib)
    dnl PKG_CHECK_MODULES(DNL, dnl-lib)
    AC_CHECK_LIB([xxhash], [XXH64], [XXH_LIBS="-lxxhash"])
    AC_SEARCH_LIBS(clock_gettime, rt)
    AC_SEARCH_LIBS([gss_init], [gssapi_krb5 gss])
    GLIB_LIBS="$(pkg-config --libs glib-2.0)"
    ls -la /nonexistent
    """)

PLUGIN_MAKEFILE_AM = textwrap.dedent("""\
    demo_la_LDFLAGS = -module -l:libkafka-static.a -lsasl2
    demo_la_CPPFLAGS = -lnotalinkline
    """)

TESTS_MAKEFILE_AM = "check_PROGRAMS = t\nt_LDADD = -ltestonlylib\n"

MANIFEST = textwrap.dedent("""\
    [project]
    name = "demo"
    supplier = "Demo project"
    website = "https://demo.example.org"
    vcs = "https://github.com/demo/demo"
    advisories = "https://github.com/demo/demo/security/advisories"
    purl = "pkg:github/demo/demo"
    license_expression = "Apache-2.0"
    tag_prefix = "v"

    [ignore]
    names = ["rt"]

    [[vendored]]
    name = "hashlib-copy"
    license = "BSD-3-Clause"
    paths = ["runtime/vendored.c"]

    [[vendored]]
    name = "pd-snippet"
    license_name = "Public Domain"
    paths = ["runtime/vendored.c"]

    [[vendored]]
    name = "d3"
    version = "7.9.0"
    license = "BSD-3-Clause"
    purl = "pkg:npm/d3@7.9.0"
    paths = ["doc/d3.min.js"]

    [[dependency]]
    name = "libestr"
    match = ["libestr"]
    required = true
    purl = "pkg:github/rsyslog/libestr"

    [[dependency]]
    name = "libevent"
    match = ["libevent", "libevent_pthreads"]

    [[dependency]]
    name = "libcap-ng"
    match = ["libcap-ng"]

    [[dependency]]
    name = "xxhash"
    match = ["xxhash"]

    [[dependency]]
    name = "krb5"
    match = ["gssapi_krb5", "gss"]

    [[dependency]]
    name = "glib"
    match = ["glib-2.0"]

    [[dependency]]
    name = "kafka"
    match = ["kafka"]

    [[dependency]]
    name = "cyrus-sasl"
    match = ["sasl2"]

    [[dependency]]
    name = "tcl"
    match = []
    declared = true
    """)

VENDOR_README = textwrap.dedent("""\
    | Package | Version | Upstream source | License |
    |---------|---------|-----------------|---------|
    | d3 | 7.9.0 | https://example.org/d3.min.js | BSD-3-Clause |
    """)


class FixtureTree:
    """A minimal, consistent source tree in a temporary directory."""

    def __init__(self):
        self._tmp = tempfile.TemporaryDirectory()
        self.root = self._tmp.name
        self.write("configure.ac", CONFIGURE_AC)
        self.write("plugins/demo/Makefile.am", PLUGIN_MAKEFILE_AM)
        self.write("tests/Makefile.am", TESTS_MAKEFILE_AM)
        self.write("runtime/vendored.c", "/* vendored */\n")
        self.write("doc/d3.min.js", "/* d3 */\n")
        self.write("doc/source/_static/vendor/README.md", VENDOR_README)
        self.write("sbom-components.toml", MANIFEST)

    def write(self, rel, text):
        path = os.path.join(self.root, rel)
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "w", encoding="utf-8") as fh:
            fh.write(text)

    def replace(self, rel, old, new):
        path = os.path.join(self.root, rel)
        with open(path, encoding="utf-8") as fh:
            text = fh.read()
        assert old in text, (rel, old)
        self.write(rel, text.replace(old, new))

    @property
    def manifest(self):
        return os.path.join(self.root, "sbom-components.toml")

    def cleanup(self):
        self._tmp.cleanup()


def run_cli(*args, env_extra=None):
    """Run generator main() in-process; return (exit code, stdout, stderr)."""
    out, err = io.StringIO(), io.StringIO()
    old_env = dict(os.environ)
    try:
        os.environ.update(env_extra or {})
        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
            rc = gen.main(list(args))
    finally:
        os.environ.clear()
        os.environ.update(old_env)
    return rc, out.getvalue(), err.getvalue()


class ScanTests(unittest.TestCase):
    def setUp(self):
        self.tree = FixtureTree()
        _, self.found = gen.scan_tree(self.tree.root)

    def tearDown(self):
        self.tree.cleanup()

    def test_all_library_forms_found(self):
        expected = {"libestr", "libevent", "libevent_pthreads", "libcap-ng", "xxhash", "rt",
                    "gssapi_krb5", "gss", "glib-2.0", "kafka", "sasl2"}
        self.assertEqual(set(self.found), expected)

    def test_version_constraints_kept(self):
        self.assertEqual(self.found["libestr"]["constraints"], {">= 0.1.9"})
        self.assertEqual(self.found["libevent"]["constraints"], {">= 2.0"})
        self.assertEqual(self.found["libevent_pthreads"]["constraints"], set())
        self.assertEqual(self.found["libcap-ng"]["constraints"], {">= 0.8.2"})

    def test_comments_tests_and_non_link_text_ignored(self):
        for name in ("commented-out-lib", "dnl-lib", "testonlylib", "a", "notalinkline"):
            self.assertNotIn(name, self.found)

    def test_link_name_normalizes_ld_file_form(self):
        self.assertEqual(gen.link_name(":librdkafka-static.a"), "rdkafka")
        self.assertEqual(gen.link_name(":libqpid-proton-core-static.a"), "qpid-proton-core")
        self.assertEqual(gen.link_name(":libfoo.so.1.2"), "foo")
        self.assertEqual(gen.link_name("z"), "z")

    def test_ac_init_version(self):
        self.assertEqual(gen.read_ac_init_version(CONFIGURE_AC), "1.2.3")


class CheckTests(unittest.TestCase):
    def setUp(self):
        self.tree = FixtureTree()

    def tearDown(self):
        self.tree.cleanup()

    def check(self):
        return run_cli("--srcdir", self.tree.root, "--manifest", self.tree.manifest, "--check")

    def test_consistent_tree_passes(self):
        rc, out, err = self.check()
        self.assertEqual(rc, 0, err)
        self.assertIn("sbom manifest OK", out)

    def test_unknown_library_fails(self):
        self.tree.replace("configure.ac", "AC_SEARCH_LIBS(clock_gettime, rt)",
                          "AC_SEARCH_LIBS(clock_gettime, rt)\nPKG_CHECK_MODULES(NEW, newlib >= 1)")
        rc, _, err = self.check()
        self.assertEqual(rc, 1)
        self.assertIn("library 'newlib'", err)

    def test_unused_dependency_fails(self):
        self.tree.replace("configure.ac", 'GLIB_LIBS="$(pkg-config --libs glib-2.0)"\n', "")
        rc, _, err = self.check()
        self.assertEqual(rc, 1)
        self.assertIn("dependency 'glib' is no longer used", err)

    def test_missing_vendored_path_fails(self):
        os.remove(os.path.join(self.tree.root, "runtime/vendored.c"))
        rc, _, err = self.check()
        self.assertEqual(rc, 1)
        self.assertIn("path 'runtime/vendored.c' does not exist", err)

    def test_vendored_version_mismatch_fails(self):
        self.tree.replace("doc/source/_static/vendor/README.md", "| 7.9.0 |", "| 7.9.1 |")
        rc, _, err = self.check()
        self.assertEqual(rc, 1)
        self.assertIn("version 7.9.0 differs", err)

    def test_duplicate_match_is_an_error(self):
        self.tree.replace("sbom-components.toml", 'match = ["sasl2"]', 'match = ["sasl2", "kafka"]')
        rc, _, err = self.check()
        self.assertEqual(rc, 1)
        self.assertIn("'kafka' is matched by both", err)


class OutputTests(unittest.TestCase):
    EPOCH = {"SOURCE_DATE_EPOCH": "1764000000"}

    def setUp(self):
        self.tree = FixtureTree()
        self.tarball = os.path.join(self.tree.root, "demo-1.2.3.tar.gz")
        with open(self.tarball, "wb") as fh:
            fh.write(b"demo tarball contents\n")

    def tearDown(self):
        self.tree.cleanup()

    def generate(self):
        rc, out, err = run_cli("--srcdir", self.tree.root, "--manifest", self.tree.manifest,
                               "--tarball", self.tarball, env_extra=self.EPOCH)
        self.assertEqual(rc, 0, err)
        return out

    def test_document_structure(self):
        bom = json.loads(self.generate())
        self.assertEqual(bom["bomFormat"], "CycloneDX")
        self.assertEqual(bom["specVersion"], "1.6")
        self.assertTrue(bom["serialNumber"].startswith("urn:uuid:"))
        self.assertEqual(bom["metadata"]["timestamp"], "2025-11-24T16:00:00Z")
        main = bom["metadata"]["component"]
        self.assertEqual(main["name"], "demo")
        self.assertEqual(main["version"], "1.2.3")
        self.assertEqual(main["purl"], "pkg:github/demo/demo@v1.2.3")
        digest = hashlib.sha256(b"demo tarball contents\n").hexdigest()
        self.assertEqual(main["hashes"], [{"alg": "SHA-256", "content": digest}])

    def test_components_and_dependencies(self):
        bom = json.loads(self.generate())
        comps = {c["name"]: c for c in bom["components"]}
        # rt is ignored; tcl is declared; everything else is scanned.
        self.assertEqual(set(comps), {"hashlib-copy", "pd-snippet", "d3", "libestr", "libevent", "libcap-ng", "xxhash",
                                      "krb5", "glib", "kafka", "cyrus-sasl", "tcl"})
        self.assertEqual(comps["libestr"]["scope"], "required")
        self.assertEqual(comps["libevent"]["scope"], "optional")
        self.assertIn({"name": "rsyslog:sbom:version-constraint", "value": ">= 0.1.9"},
                      comps["libestr"]["properties"])
        self.assertEqual(comps["hashlib-copy"]["scope"], "required")
        self.assertEqual(comps["hashlib-copy"]["evidence"]["occurrences"], [{"location": "runtime/vendored.c"}])
        self.assertEqual(comps["d3"]["purl"], "pkg:npm/d3@7.9.0")
        self.assertEqual(comps["d3"]["licenses"], [{"license": {"id": "BSD-3-Clause"}}])
        self.assertEqual(comps["pd-snippet"]["licenses"], [{"license": {"name": "Public Domain"}}])
        refs = [c["bom-ref"] for c in bom["components"]]
        self.assertEqual(len(refs), len(set(refs)), "bom-refs must be unique")
        known = set(refs) | {bom["metadata"]["component"]["bom-ref"]}
        for dep in bom["dependencies"]:
            self.assertIn(dep["ref"], known)
            for target in dep["dependsOn"]:
                self.assertIn(target, known)

    def test_output_is_reproducible(self):
        self.assertEqual(self.generate(), self.generate())

    def test_check_failure_does_not_block_generation(self):
        # Release builds must still produce an SBOM; unknown libraries stay
        # visible as components and are reported as warnings.
        self.tree.replace("configure.ac", "AC_SEARCH_LIBS(clock_gettime, rt)",
                          "AC_SEARCH_LIBS(clock_gettime, rt)\nPKG_CHECK_MODULES(NEW, newlib)")
        rc, out, err = run_cli("--srcdir", self.tree.root, "--manifest", self.tree.manifest,
                               env_extra=self.EPOCH)
        self.assertEqual(rc, 0)
        self.assertIn("WARNING: library 'newlib'", err)
        self.assertIn("newlib", [c["name"] for c in json.loads(out)["components"]])


OPTIONS_CONFIGURE_AC = textwrap.dedent("""\
    AC_INIT([demo],[1.0.0],[demo@example.org])
    AC_ARG_ENABLE(omdemo,
            [AS_HELP_STRING([--enable-omdemo],[demo output])],
            [enable_omdemo="${enableval}"],
            [enable_omdemo=no])
    AC_ARG_ENABLE(libcap-ng,
            [AS_HELP_STRING([--enable-libcap-ng],[capabilities])],
            [case "${enableval}" in
             yes) enable_libcapng="yes" ;;
             esac],
            [enable_libcapng=no])
    AC_ARG_ENABLE(imdemo, [AS_HELP_STRING([--enable-imdemo],[demo input])])
    AC_ARG_ENABLE(direct, [AS_HELP_STRING([--enable-direct],[x])],
            [PKG_CHECK_MODULES(DIRECT, directlib)])
    AC_ARG_WITH(fancy_lib, [AS_HELP_STRING([--with-fancy-lib],[x])])
    AC_DEFUN([DEMO_NEEDS_SSL], [
            PKG_CHECK_MODULES(SSL, openssl)
    ])
    if test "x$enable_omdemo" = "xyes"; then
            PKG_CHECK_MODULES(KAFKA, rdkafka >= 1.0)
            DEMO_NEEDS_SSL
            AC_TRY_LINK(
              [#include <stdio.h>
               #include <stdlib.h>],
              [return 0;])
    elif test "x$enable_imdemo" = "xyes"; then
            PKG_CHECK_MODULES(KAFKA, rdkafka)
    else
            PKG_CHECK_MODULES(ELSEONLY, elseonly)
    fi
    if test "$enable_libcapng" = "yes"; then PKG_CHECK_MODULES(CAPNG, libcap-ng); fi
    AS_IF([test "x${with_fancy_lib}" = "xyes"], [
            PKG_CHECK_MODULES(FANCY, fancy)
    ], [
            PKG_CHECK_MODULES(NOTFANCY, notfancy)
    ])
    PKG_CHECK_MODULES(ESTR, libestr)
    if test "x$enable_omdemo" = "xyes"; then
            PKG_CHECK_MODULES(UUID, uuid)
    fi
    PKG_CHECK_MODULES(UUID, uuid)
    AC_ARG_ENABLE(omtcl, [AS_HELP_STRING([--enable-omtcl],[tcl])])
    """)

OPTIONS_MANIFEST = textwrap.dedent("""\
    [project]
    name = "demo"
    supplier = "Demo project"
    website = "https://demo.example.org"
    vcs = "https://github.com/demo/demo"
    advisories = "https://github.com/demo/demo/security/advisories"
    purl = "pkg:github/demo/demo"
    license_expression = "Apache-2.0"

    [[dependency]]
    name = "librdkafka"
    match = ["rdkafka"]

    [[dependency]]
    name = "openssl"
    match = ["openssl"]

    [[dependency]]
    name = "libcap-ng"
    match = ["libcap-ng"]

    [[dependency]]
    name = "direct"
    match = ["directlib"]

    [[dependency]]
    name = "fancy"
    match = ["fancy"]

    [[dependency]]
    name = "notfancy"
    match = ["notfancy"]

    [[dependency]]
    name = "elseonly"
    match = ["elseonly"]

    [[dependency]]
    name = "libestr"
    match = ["libestr"]
    required = true

    [[dependency]]
    name = "libuuid"
    match = ["uuid"]

    [[dependency]]
    name = "libpcap"
    match = ["pcap"]

    [[dependency]]
    name = "tcl"
    match = []
    declared = true
    configure_options = ["--enable-omtcl"]
    """)


class OptionTests(unittest.TestCase):
    def setUp(self):
        self.tree = FixtureTree()
        self.tree.write("configure.ac", OPTIONS_CONFIGURE_AC)
        self.tree.write("plugins/demo/Makefile.am", "")
        self.tree.write("plugins/imdemo/Makefile.am", "imdemo_la_LIBADD = -lpcap\n")
        self.tree.write("sbom-components.toml", OPTIONS_MANIFEST)

    def tearDown(self):
        self.tree.cleanup()

    def bom_options(self):
        rc, out, err = run_cli("--srcdir", self.tree.root, "--manifest", self.tree.manifest,
                               env_extra={"SOURCE_DATE_EPOCH": "1764000000"})
        self.assertEqual(rc, 0, err)
        result = {}
        for c in json.loads(out)["components"]:
            props = c.get("properties", [])
            result[c["name"]] = (
                [p["value"] for p in props if p["name"] == "rsyslog:sbom:configure-option"],
                any(p["name"] == "rsyslog:sbom:probed-without-option" for p in props))
        return result

    def test_declared_options_include_assigned_variables(self):
        options = gen.declared_options(OPTIONS_CONFIGURE_AC)
        self.assertEqual(options["enable_omdemo"], "--enable-omdemo")
        self.assertEqual(options["enable_libcap_ng"], "--enable-libcap-ng")
        self.assertEqual(options["enable_libcapng"], "--enable-libcap-ng")
        self.assertEqual(options["with_fancy_lib"], "--with-fancy-lib")

    def test_options_per_library(self):
        opts = self.bom_options()
        # if and elif branches both pull in rdkafka.
        self.assertEqual(opts["librdkafka"], (["--enable-imdemo", "--enable-omdemo"], False))
        # AC_DEFUN body inherits the guard of its call site.
        self.assertEqual(opts["openssl"], (["--enable-omdemo"], False))
        # One-line if with a variable the option's action assigns.
        self.assertEqual(opts["libcap-ng"], (["--enable-libcap-ng"], False))
        # Probe inside the action-if-given of AC_ARG_ENABLE.
        self.assertEqual(opts["direct"], (["--enable-direct"], False))
        # AS_IF then-branch with ${with_...}.
        self.assertEqual(opts["fancy"], (["--with-fancy-lib"], False))
        # plugins/imdemo/Makefile.am maps to --enable-imdemo.
        self.assertEqual(opts["libpcap"], (["--enable-imdemo"], False))
        # Manifest-declared option for a library the scanner cannot see.
        self.assertEqual(opts["tcl"], (["--enable-omtcl"], False))

    def test_else_branches_and_required_are_not_attributed(self):
        opts = self.bom_options()
        self.assertEqual(opts["notfancy"], ([], False))
        self.assertEqual(opts["elseonly"], ([], False))
        self.assertEqual(opts["libestr"], ([], False))

    def test_unconditional_probe_is_flagged(self):
        self.assertEqual(self.bom_options()["libuuid"], (["--enable-omdemo"], True))

    def test_quoted_c_code_keeps_quotes_balanced(self):
        # ' #include <stdlib.h>],' looks like a shell comment; dropping its
        # ']' would swallow the rest of the file into the AC_TRY_LINK call.
        clean = gen.strip_comments(OPTIONS_CONFIGURE_AC)
        start = clean.index("AC_TRY_LINK(")
        args = gen.macro_args(clean, start + len("AC_TRY_LINK"))
        self.assertEqual(args[1], "return 0;")

    def test_unknown_configure_option_fails_check(self):
        self.tree.replace("sbom-components.toml", '"--enable-omtcl"', '"--enable-nosuch"')
        rc, _, err = run_cli("--srcdir", self.tree.root, "--manifest", self.tree.manifest, "--check")
        self.assertEqual(rc, 1)
        self.assertIn("configure option '--enable-nosuch'", err)


class RealTreeTests(unittest.TestCase):
    def test_manifest_matches_this_source_tree(self):
        proc = subprocess.run([sys.executable, GENERATOR, "--srcdir", SRCDIR, "--check"],
                              capture_output=True, text=True, check=False)
        self.assertEqual(proc.returncode, 0, proc.stderr)

    def test_real_tree_sbom_lists_core_dependencies(self):
        proc = subprocess.run([sys.executable, GENERATOR, "--srcdir", SRCDIR, "--version", "8.0.0"],
                              capture_output=True, text=True, check=False)
        self.assertEqual(proc.returncode, 0, proc.stderr)
        comps = {c["name"]: c for c in json.loads(proc.stdout)["components"]}
        for name in ("libestr", "libfastjson", "zlib"):
            self.assertEqual(comps[name]["scope"], "required", name)

        def options(name):
            return {p["value"] for p in comps[name]["properties"]
                    if p["name"] == "rsyslog:sbom:configure-option"}
        self.assertTrue({"--enable-omkafka", "--enable-imkafka"} <= options("librdkafka"))
        self.assertIn("--enable-mysql", options("mysqlclient"))
        self.assertIn("--enable-omamqp1", options("openssl"))


if __name__ == "__main__":
    unittest.main(verbosity=2)
