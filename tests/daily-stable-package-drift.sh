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
