#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 Rainer Gerhards and Others
#
# This file is part of the rsyslog project, released under ASL 2.0.
# Verify the daily-stable safeguards against native-baseline drift:
#
# - upstream_patch_detect.py recognizes distribution backports from each
#   supported header form (DEP-3 Origin, backport note, subject match, and
#   abbreviated hashes) and rejects short generic subjects, ambiguous
#   abbreviations, free-text DEP-3 commit mentions, and reverted commits.  The oracle is the CLI exit status.
# - The Debian quilt loop skips an unlisted patch that fails to apply only when
#   the detector proves it upstream; an unproven failing patch still fails.
#   This part needs quilt and the .github CI helper and is skipped without them.
# - A checksum-pinned Debian schema patch refresh preserves database ownership,
#   idempotent MySQL tables, and upstream BIGINT IDs, and rejects native drift.
# - rpm-disable-optional-modules.py disables listed modules after %configure
#   and rejects a native spec that enables one explicitly, while accepting
#   the --enable-MODULE=no disable spelling.
# - check-optional-module-policy.py accepts the repository and rejects an
#   auto-enabled configure option that no target policy covers, so a new
#   module fails its own pull request instead of the nightly package builds.
#   The policies are not distributed, so a distribution tarball skips this.
set -eu

test_dir=$(CDPATH='' cd -- "$(dirname "$0")" && pwd)
repo_dir="$test_dir/.."
detect="$repo_dir/devtools/release/upstream_patch_detect.py"
disable="$repo_dir/devtools/release/rpm-disable-optional-modules.py"
guard="$repo_dir/devtools/release/check-optional-module-policy.py"
work_dir="./daily-stable-package-drift-work.$$"

command -v python3 >/dev/null 2>&1 || exit 77

cleanup() {
	rm -rf -- "$work_dir"
}
trap cleanup EXIT HUP INT TERM
mkdir -p "$work_dir"

index="$work_dir/upstream-commits.txt"
# Two hashes share the prefix aaaaaaaaaa to exercise ambiguous abbreviations.
printf '%s %s\n' \
	aaaaaaaaaa111111111111111111111111111111 'rainerscript: align replace sizing rewind' \
	aaaaaaaaaa222222222222222222222222222222 'fix build' \
	bbbbbbbbbb333333333333333333333333333333 'mmpstrucdata: support custom SD containers' \
	cccccccccc444444444444444444444444444444 'Revert "imfile: experimental watch mode"' \
	dddddddddd555555555555555555555555555555 'imfile: experimental watch mode' \
	>"$index"

expect_detect() {
	expected=$1
	shift
	printf '%b' "$*" >"$work_dir/case.patch"
	if python3 "$detect" "$index" "$work_dir/case.patch" >"$work_dir/case.out"; then
		actual=yes
	else
		actual=no
	fi
	if [ "$actual" != "$expected" ]; then
		echo "detector said '$actual', expected '$expected' for: $*" >&2
		cat "$work_dir/case.out" >&2
		exit 1
	fi
}

expect_detect yes 'Origin: upstream, https://github.com/rsyslog/rsyslog/commit/bbbbbbbbbb33\n---\n'
expect_detect yes 'Subject: mmpstrucdata fix\n\nPartial backport of commit bbbbbbbbbb333333\n---\n'
expect_detect yes 'From: someone\nSubject: [PATCH 2/3] rainerscript: align replace\n sizing rewind\n---\n'
expect_detect no 'Subject: fix build\n---\n'
expect_detect no 'Subject: other\n\n(cherry picked from commit aaaaaaaaaa)\n---\n'
expect_detect no 'Subject: local tweak\n---\n'
# A free-text commit mention inside a DEP-3 field is not an origin claim.
expect_detect no 'Origin: vendor, fixes a regression from commit bbbbbbbbbb33\n---\n'
# A commit that main later reverted is not integrated, by reference or subject.
expect_detect no 'Subject: x\n\n(cherry picked from commit dddddddddd55)\n---\n'
expect_detect no 'Subject: imfile: experimental watch mode\n---\n'
# References after the header (inside the diff) are not evidence.
expect_detect no 'Subject: local tweak\n---\ndiff --git a/x b/x\n+(cherry picked from commit bbbbbbbbbb33)\n'

debian_helper="$repo_dir/.github/scripts/debian_package_build.sh"
if [ ! -f "$debian_helper" ]; then
	echo "Debian CI helper not present (distribution tarball); skipping Debian patch resolution checks"
elif command -v quilt >/dev/null 2>&1; then
	# resolve_patch_policy changes into the source tree; pass absolute paths.
	abs_work=$(cd "$work_dir" && pwd)
	src="$abs_work/debian-src"
	mkdir -p "$src/debian/patches"
	printf 'old\n' >"$src/file.txt"
	printf 'upstream\n' >"$src/fixed.txt"
	printf -- '--- a/file.txt\n+++ b/file.txt\n@@ -1 +1 @@\n-old\n+new\n' \
		>"$src/debian/patches/applies.patch"
	printf 'Subject: fixed\n\n(cherry picked from commit bbbbbbbbbb33)\n---\n--- a/fixed.txt\n+++ b/fixed.txt\n@@ -1 +1 @@\n-before\n+upstream\n' \
		>"$src/debian/patches/backport.patch"
	printf 'applies.patch\nbackport.patch\n' >"$src/debian/patches/series"
	: >"$work_dir/allowed.txt"
	: >"$work_dir/reasons.txt"
	output=$(RSYSLOG_UPSTREAM_COMMIT_INDEX="$abs_work/upstream-commits.txt" \
		bash "$debian_helper" resolve_patch_policy \
		"$src" "$abs_work/allowed.txt" "$abs_work/reasons.txt" strict 2>&1)
	printf '%s\n' "$output" | grep -F '::warning title=Debian patch already upstream::skipped backport.patch' >/dev/null
	test "$(cat "$src/debian/patches/series")" = 'applies.patch'

	printf 'Subject: local\n---\n--- a/fixed.txt\n+++ b/fixed.txt\n@@ -1 +1 @@\n-before\n+local\n' \
		>"$src/debian/patches/local.patch"
	printf 'applies.patch\nlocal.patch\n' >"$src/debian/patches/series"
	if RSYSLOG_UPSTREAM_COMMIT_INDEX="$abs_work/upstream-commits.txt" \
		bash "$debian_helper" resolve_patch_policy \
		"$src" "$abs_work/allowed.txt" "$abs_work/reasons.txt" strict >"$work_dir/quilt.out" 2>&1; then
		echo "unproven failing Debian patch was skipped" >&2
		exit 1
	fi
	grep -F "unexpected Debian patch failure 'local.patch'" "$work_dir/quilt.out" >/dev/null
else
	echo "quilt not installed; skipping Debian patch resolution checks"
fi


# The real Debian 13 patch must still apply after upstream widened MySQL IDs.
# The oracle checks applied SQL semantics, preserved BIGINT columns, unchanged
# series membership, and rejection of even harmless native-patch drift.
if [ -f "$repo_dir/.github/debian-daily-stable-13-policy.yml" ] && command -v quilt >/dev/null 2>&1; then
    abs_work=$(cd "$work_dir" && pwd)
    schema_src="$abs_work/schema-src"
    mkdir -p "$schema_src/debian/patches" "$schema_src/plugins/ommysql" "$schema_src/plugins/ompgsql"
    cp "$repo_dir/plugins/ommysql/createDB.sql" "$schema_src/plugins/ommysql/"
    cp "$repo_dir/plugins/ompgsql/createDB.sql" "$schema_src/plugins/ompgsql/"
    cat >"$schema_src/debian/patches/Don-t-create-a-database.patch" <<'NATIVE_PATCH'
From: Michael Biebl <biebl@debian.org>
Date: Thu, 13 Mar 2014 17:58:08 +0100
Subject: Don't create a database

dbconfig-common, which is used by rsyslog-mysql and rsyslog-pgsql, takes
care of creating the database for us.
---
 plugins/ommysql/createDB.sql | 6 ++----
 plugins/ompgsql/createDB.sql | 2 --
 2 files changed, 2 insertions(+), 6 deletions(-)

diff --git a/plugins/ommysql/createDB.sql b/plugins/ommysql/createDB.sql
index 6261284..9dc48de 100644
--- a/plugins/ommysql/createDB.sql
+++ b/plugins/ommysql/createDB.sql
@@ -1,6 +1,4 @@
-CREATE DATABASE Syslog;
-USE Syslog;
-CREATE TABLE SystemEvents
+CREATE TABLE IF NOT EXISTS SystemEvents
 (
         ID int unsigned not null auto_increment primary key,
         CustomerID bigint,
@@ -28,7 +26,7 @@ CREATE TABLE SystemEvents
         SystemID int NULL
 );
@
-CREATE TABLE SystemEventsProperties
+CREATE TABLE IF NOT EXISTS SystemEventsProperties
 (
         ID int unsigned not null auto_increment primary key,
         SystemEventID int NULL ,
diff --git a/plugins/ompgsql/createDB.sql b/plugins/ompgsql/createDB.sql
index 2f1ade0..879a1b5 100644
--- a/plugins/ompgsql/createDB.sql
+++ b/plugins/ompgsql/createDB.sql
@@ -1,5 +1,3 @@
-CREATE DATABASE "Syslog" WITH ENCODING 'SQL_ASCII' TEMPLATE template0;
-\c Syslog;
 CREATE TABLE SystemEvents
 (
         ID serial not null primary key,
NATIVE_PATCH
    # Restore quilt's blank context line without source-file trailing whitespace.
    sed -i 's/^@$/ /' "$schema_src/debian/patches/Don-t-create-a-database.patch"
    printf '%s\n' Don-t-create-a-database.patch >"$schema_src/debian/patches/series"
    cp "$schema_src/debian/patches/Don-t-create-a-database.patch" "$abs_work/native-schema.patch"
    bash "$debian_helper" load_policy "$repo_dir/.github/debian-daily-stable-13-policy.yml" "$abs_work/schema-policy"
    # First reproduce the failure without the policy refresh; no allowlist entry
    # may hide this required downstream patch.
    if bash "$debian_helper" resolve_patch_policy "$schema_src" "$abs_work/allowed.txt" \
        "$abs_work/reasons.txt" strict >"$work_dir/schema-before.out" 2>&1; then
        echo "unrefreshed native schema patch unexpectedly applied" >&2
        exit 1
    fi
    grep -F "unexpected Debian patch failure 'Don-t-create-a-database.patch'" "$work_dir/schema-before.out" >/dev/null
    bash "$debian_helper" resolve_patch_policy "$schema_src" \
        "$abs_work/schema-policy/allowed_patch_skips.txt" \
        "$abs_work/schema-policy/allowed_patch_skips.reasons.txt" strict
    test "$(cat "$schema_src/debian/patches/series")" = Don-t-create-a-database.patch
    (cd "$schema_src" && QUILT_PATCHES=debian/patches quilt push --fuzz=0 -a)
    ! grep -E '^(CREATE DATABASE|USE |\\c )' "$schema_src"/plugins/*/createDB.sql
    test "$(grep -c '^CREATE TABLE IF NOT EXISTS ' "$schema_src/plugins/ommysql/createDB.sql")" -eq 2
    test "$(grep -c 'ID bigint unsigned not null auto_increment primary key' "$schema_src/plugins/ommysql/createDB.sql")" -eq 2
    grep -F 'SystemEventID bigint unsigned NULL' "$schema_src/plugins/ommysql/createDB.sql" >/dev/null
    (cd "$schema_src" && QUILT_PATCHES=debian/patches quilt pop -a)
    cp "$abs_work/native-schema.patch" "$schema_src/debian/patches/Don-t-create-a-database.patch"
    printf '\n' >>"$schema_src/debian/patches/Don-t-create-a-database.patch"
    if bash "$debian_helper" resolve_patch_policy "$schema_src" \
        "$abs_work/schema-policy/allowed_patch_skips.txt" \
        "$abs_work/schema-policy/allowed_patch_skips.reasons.txt" strict >"$work_dir/schema-drift.out" 2>&1; then
        echo "changed native schema patch was silently refreshed" >&2
        exit 1
    fi
    grep -F 'native Debian patch changed; review refresh' "$work_dir/schema-drift.out" >/dev/null
    cp "$abs_work/native-schema.patch" "$schema_src/debian/patches/Don-t-create-a-database.patch"
    : >"$schema_src/debian/patches/series"
    if bash "$debian_helper" resolve_patch_policy "$schema_src" \
        "$abs_work/schema-policy/allowed_patch_skips.txt" \
        "$abs_work/schema-policy/allowed_patch_skips.reasons.txt" strict >"$work_dir/schema-absent.out" 2>&1; then
        echo "stale native schema refresh was accepted" >&2
        exit 1
    fi
    grep -F 'reviewed Debian patch no longer in series' "$work_dir/schema-absent.out" >/dev/null
fi

spec="$work_dir/rsyslog.spec"
printf '%s\n' '%build' '%configure \' '	--prefix=/usr \' '	--enable-omhttp' >"$spec"
printf '%s\n' '{"disabled_optional_modules": [{"module": "omkinesis", "reason": "fixture"}]}' \
	>"$work_dir/policy.json"
python3 "$disable" "$spec" "$work_dir/policy.json" test
sed -n 3p "$spec" | grep -Fx '	--disable-omkinesis \' >/dev/null
python3 "$disable" "$spec" "$work_dir/policy.json" test
test "$(grep -c -- '--disable-omkinesis' "$spec")" -eq 1
for enabling in '--enable-omkinesis' '--enable-omkinesis=yes' '--enable-omkinesis=optional'; do
	printf '%s\n' '%configure \' "	$enabling" >"$spec"
	if python3 "$disable" "$spec" "$work_dir/policy.json" test 2>/dev/null; then
		echo "native spec enabling a disabled optional module was accepted: $enabling" >&2
		exit 1
	fi
done
# Autoconf treats --enable-MODULE=no as a disable; it is not a conflict.
printf '%s\n' '%configure \' '	--enable-omkinesis=no' >"$spec"
python3 "$disable" "$spec" "$work_dir/policy.json" test

# The daily-stable policies live in .github/, which only a git checkout has.
if [ ! -f "$repo_dir/.github/daily-stable-package-features.yml" ]; then
	echo "daily-stable policies not present (distribution tarball); skipping policy guard checks"
	exit 0
fi
python3 "$guard" "$repo_dir" >/dev/null
fixture="$work_dir/repo"
mkdir -p "$fixture/.github"
cp "$repo_dir"/.github/*-daily-stable*-policy.yml "$repo_dir/.github/daily-stable-package-features.yml" \
	"$fixture/.github/"
{
	cat "$repo_dir/configure.ac"
	printf '%s\n' 'AC_ARG_ENABLE(omfixture,' \
		'        [AS_HELP_STRING([--enable-omfixture],[fixture @<:@default=optional@:>@])],' \
		'        [enable_omfixture="${enableval}"],' \
		'        [enable_omfixture=optional])'
} >"$fixture/configure.ac"
if python3 "$guard" "$fixture" 2>"$work_dir/guard.err"; then
	echo "uncovered auto-enabled module was accepted" >&2
	exit 1
fi
grep -F 'fedora44-daily-stable-policy.yml: add omfixture to disabled_optional_modules' "$work_dir/guard.err" >/dev/null
grep -F 'debian-daily-stable-13-policy.yml: add usr/lib/*/rsyslog/omfixture.so' "$work_dir/guard.err" >/dev/null
