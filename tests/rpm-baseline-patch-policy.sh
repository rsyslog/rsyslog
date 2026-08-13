#!/bin/sh
# Verify RPM daily-stable patch policies are exact and content-pinned.  A
# reviewed patch must be removed from both its source and explicit %patch
# application, while an unreviewed baseline patch must reject packaging before
# a build can apply a stale downstream change to current upstream sources.
set -eu

test_dir=$(CDPATH='' cd -- "$(dirname "$0")" && pwd)
helper="$test_dir/../devtools/release/validate-rpm-baseline-patches.py"
work_dir="./rpm-baseline-patch-policy-work.$$"

cleanup() {
	rm -rf -- "$work_dir"
}
trap cleanup EXIT HUP INT TERM

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
digest=$(sha256sum "$work_dir/SOURCES/integrated.patch" | awk '{print $1}')
legacy_digest=$(sha256sum "$work_dir/SOURCES/legacy-integrated.patch" | awk '{print $1}')
cat >"$work_dir/policy.json" <<EOF
{
  "integrated_baseline_patches": [
    {
      "name": "integrated.patch",
      "sha256": "$digest",
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
