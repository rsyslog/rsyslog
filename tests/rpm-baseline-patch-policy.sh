#!/bin/sh
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 Rainer Gerhards and Others
#
# This file is part of the rsyslog project, released under ASL 2.0.
# Verify RPM daily-stable patch policies are exact and content-pinned.  A
# reviewed patch must be removed from both its source and explicit %patch
# application, while an unreviewed baseline patch must reject packaging before
# a build can apply a stale downstream change to current upstream sources.
set -eu

test_dir=$(CDPATH='' cd -- "$(dirname "$0")" && pwd)
helper="$test_dir/../devtools/release/validate-rpm-baseline-patches.py"
work_dir="./rpm-baseline-patch-policy-work.$$"

command -v python3 >/dev/null 2>&1 || exit 77

cleanup() {
	rm -rf -- "$work_dir"
}
trap cleanup EXIT HUP INT TERM

digest() {
	python3 - "$1" <<'PY'
import hashlib
import pathlib
import sys

print(hashlib.sha256(pathlib.Path(sys.argv[1]).read_bytes()).hexdigest())
PY
}

mkdir -p "$work_dir/SPECS" "$work_dir/SOURCES"
cat >"$work_dir/SPECS/rsyslog.spec" <<'EOF'
Name: rsyslog
Patch4: integrated.patch
Patch5: legacy-integrated.patch
%prep
%patch -P4 -p1
%patch5 -p1
EOF
printf '%s\n' 'reviewed patch content' >"$work_dir/SOURCES/integrated.patch"
printf '%s\n' 'reviewed legacy patch content' >"$work_dir/SOURCES/legacy-integrated.patch"
integrated_digest=$(digest "$work_dir/SOURCES/integrated.patch")
legacy_digest=$(digest "$work_dir/SOURCES/legacy-integrated.patch")
cat >"$work_dir/policy.json" <<EOF
{
  "integrated_baseline_patches": [
    {
      "name": "integrated.patch",
      "sha256": "$integrated_digest",
      "reason": "The test fixture models an upstream-integrated patch."
    },
    {
      "name": "legacy-integrated.patch",
      "sha256": "$legacy_digest",
      "reason": "The test fixture models the compact RPM patch macro form."
    }
  ]
}
EOF

python3 "$helper" "$work_dir/SPECS/rsyslog.spec" "$work_dir/policy.json" test
test ! -e "$work_dir/SOURCES/integrated.patch"
test "$(grep -c '^Patch' "$work_dir/SPECS/rsyslog.spec" || true)" -eq 0
test "$(grep -c '^%patch' "$work_dir/SPECS/rsyslog.spec" || true)" -eq 0

mkdir -p "$work_dir/reject/SPECS" "$work_dir/reject/SOURCES"
printf '%s\n' 'Patch5: unreviewed.patch' >"$work_dir/reject/SPECS/rsyslog.spec"
printf '%s\n' 'unreviewed patch content' >"$work_dir/reject/SOURCES/unreviewed.patch"
printf '%s\n' '{"integrated_baseline_patches": []}' >"$work_dir/reject/policy.json"
if result=$(python3 "$helper" "$work_dir/reject/SPECS/rsyslog.spec" "$work_dir/reject/policy.json" test 2>&1); then
	echo "unreviewed baseline patch was accepted" >&2
	exit 1
fi
printf '%s\n' "$result" | grep -F 'baseline gained unreviewed patches' >/dev/null

mkdir -p "$work_dir/unnumbered/SPECS" "$work_dir/unnumbered/SOURCES"
cat >"$work_dir/unnumbered/SPECS/rsyslog.spec" <<'EOF'
Patch: unnumbered.patch
Patch4: numbered.patch
%prep
%patch -p1
%patch -P4 -p1
EOF
printf '%s\n' 'unnumbered content' >"$work_dir/unnumbered/SOURCES/unnumbered.patch"
printf '%s\n' 'numbered content' >"$work_dir/unnumbered/SOURCES/numbered.patch"
unnumbered_digest=$(digest "$work_dir/unnumbered/SOURCES/unnumbered.patch")
numbered_digest=$(digest "$work_dir/unnumbered/SOURCES/numbered.patch")
cat >"$work_dir/unnumbered/policy.json" <<EOF
{
  "integrated_baseline_patches": [
    {"name": "unnumbered.patch", "sha256": "$unnumbered_digest", "reason": "fixture"},
    {"name": "numbered.patch", "sha256": "$numbered_digest", "reason": "fixture"}
  ]
}
EOF
python3 "$helper" "$work_dir/unnumbered/SPECS/rsyslog.spec" "$work_dir/unnumbered/policy.json" test
test ! -e "$work_dir/unnumbered/SOURCES/unnumbered.patch"
test ! -e "$work_dir/unnumbered/SOURCES/numbered.patch"
test "$(grep -c '^%patch' "$work_dir/unnumbered/SPECS/rsyslog.spec" || true)" -eq 0

variant=0
for unnumbered_application in '%patch0 -p1' '%patch 0 -p1' '%patch -P0 -p1'; do
	variant=$((variant + 1))
	fixture="$work_dir/unnumbered-zero-$variant"
	mkdir -p "$fixture/SPECS" "$fixture/SOURCES"
	{
		printf '%s\n' 'Patch: unnumbered.patch'
		printf '%%prep\n'
		printf '%s\n' "$unnumbered_application"
	} >"$fixture/SPECS/rsyslog.spec"
	printf '%s\n' 'unnumbered zero-form content' >"$fixture/SOURCES/unnumbered.patch"
	zero_digest=$(digest "$fixture/SOURCES/unnumbered.patch")
	cat >"$fixture/policy.json" <<EOF
{
  "integrated_baseline_patches": [
    {"name": "unnumbered.patch", "sha256": "$zero_digest", "reason": "fixture"}
  ]
}
EOF
	python3 "$helper" "$fixture/SPECS/rsyslog.spec" "$fixture/policy.json" test
	test ! -e "$fixture/SOURCES/unnumbered.patch"
	test "$(grep -c '^%patch' "$fixture/SPECS/rsyslog.spec" || true)" -eq 0
done

mkdir -p "$work_dir/atomic/SPECS" "$work_dir/atomic/SOURCES"
cat >"$work_dir/atomic/SPECS/rsyslog.spec" <<'EOF'
Patch0: first.patch
Patch1: changed.patch
%prep
%patch -P0 -p1
%patch -P1 -p1
EOF
printf '%s\n' 'first content' >"$work_dir/atomic/SOURCES/first.patch"
printf '%s\n' 'changed content' >"$work_dir/atomic/SOURCES/changed.patch"
first_digest=$(digest "$work_dir/atomic/SOURCES/first.patch")
cat >"$work_dir/atomic/policy.json" <<EOF
{
  "integrated_baseline_patches": [
    {"name": "first.patch", "sha256": "$first_digest", "reason": "fixture"},
    {"name": "changed.patch", "sha256": "0000000000000000000000000000000000000000000000000000000000000000", "reason": "fixture"}
  ]
}
EOF
cp "$work_dir/atomic/SPECS/rsyslog.spec" "$work_dir/atomic/original.spec"
if python3 "$helper" "$work_dir/atomic/SPECS/rsyslog.spec" "$work_dir/atomic/policy.json" test; then
	echo "changed baseline patch was accepted" >&2
	exit 1
fi
cmp "$work_dir/atomic/original.spec" "$work_dir/atomic/SPECS/rsyslog.spec"
test -f "$work_dir/atomic/SOURCES/first.patch"

mkdir -p "$work_dir/duplicate/SPECS" "$work_dir/duplicate/SOURCES"
cat >"$work_dir/duplicate/SPECS/rsyslog.spec" <<'EOF'
Patch4: first.patch
Patch04: duplicate.patch
EOF
printf '%s\n' 'first content' >"$work_dir/duplicate/SOURCES/first.patch"
printf '%s\n' 'duplicate content' >"$work_dir/duplicate/SOURCES/duplicate.patch"
first_digest=$(digest "$work_dir/duplicate/SOURCES/first.patch")
duplicate_digest=$(digest "$work_dir/duplicate/SOURCES/duplicate.patch")
cat >"$work_dir/duplicate/policy.json" <<EOF
{
  "integrated_baseline_patches": [
    {"name": "first.patch", "sha256": "$first_digest", "reason": "fixture"},
    {"name": "duplicate.patch", "sha256": "$duplicate_digest", "reason": "fixture"}
  ]
}
EOF
if result=$(python3 "$helper" "$work_dir/duplicate/SPECS/rsyslog.spec" "$work_dir/duplicate/policy.json" test 2>&1); then
	echo "equivalent patch numbers were accepted" >&2
	exit 1
fi
printf '%s\n' "$result" | grep -F 'duplicate test baseline patch declaration' >/dev/null
