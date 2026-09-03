#!/bin/sh
# SPDX-License-Identifier: Apache-2.0
#
# Copyright 2026 Rainer Gerhards and Adiscon GmbH.
#
# Regression test for the Codex pre-push gate's explicit validation bypass and
# non-runtime exemption. It proves that direct, env-prefixed, and shell-wrapped
# inline overrides are accepted, that documentation, README, and AGENTS-only
# deltas are accepted, and that ordinary and shell-wrapped pushes, false
# overrides, and mixed command lists still produce the hook's deny decision.
# Empty output is the allow oracle; the JSON permissionDecision=deny response
# is the block oracle. The shell-wrapped blocked case proves wrapper push
# recognition. The non-runtime case uses a synthetic origin/main base and
# committed documentation, README, and AGENTS files, so branch-delta
# classification is the oracle. A subsequent synthetic runtime file must return
# the gate to its blocked state, proving that the exemption cannot cover code.
# This is intentionally standalone rather than a diag.sh scenario: it exercises
# only hook command parsing and needs neither rsyslogd nor testbench helpers.
# The hook is intentionally absent from release tarballs, so this test skips
# there; source and CI checkouts retain the hook and run the assertions below.
set -eu

tmpdir="$(mktemp -d "${TMPDIR:-/tmp}/rsyslog-codex-push-gate.XXXXXX")"
trap 'rm -rf "$tmpdir"' EXIT HUP INT TERM

test_srcdir="${srcdir:-$(dirname "$0")}"
gate_source="$test_srcdir/../.codex/pre_push_container_gate.sh"
[ -f "$gate_source" ] || exit 77
command -v python3 >/dev/null 2>&1 || exit 77
command -v git >/dev/null 2>&1 || exit 77

mkdir -p "$tmpdir/.codex"
cp "$gate_source" "$tmpdir/.codex/"
gate="$tmpdir/.codex/pre_push_container_gate.sh"

run_gate() {
	command_text="$1"
	python3 -c '
import json
import sys

print(json.dumps({
    "hook_event_name": "PreToolUse",
    "tool_name": "Bash",
    "tool_input": {"command": sys.argv[1]},
}))
' "$command_text" |
		"$gate"
}

assert_allowed() {
	command_text="$1"
	output="$(run_gate "$command_text")"
	if [ -n "$output" ]; then
		printf 'expected push command to be allowed: %s\n%s\n' "$command_text" "$output" >&2
		exit 1
	fi
}

assert_blocked() {
	command_text="$1"
	output="$(run_gate "$command_text")"
	case "$output" in
		*'"permissionDecision": "deny"'*) ;;
		*)
			printf 'expected push command to be blocked: %s\n%s\n' "$command_text" "$output" >&2
			exit 1
			;;
	esac
}

assert_allowed 'SKIP_CONTAINER_VALIDATION=1 git push origin topic'
assert_allowed 'env SKIP_CONTAINER_VALIDATION=1 git push origin topic'
assert_allowed "bash -lc 'SKIP_CONTAINER_VALIDATION=1 git push origin topic'"
assert_allowed "SKIP_CONTAINER_VALIDATION=1 bash -lc 'git push origin topic'"
assert_allowed "env SKIP_CONTAINER_VALIDATION=1 sh -c 'git push origin topic'"

assert_blocked 'git push origin topic'
assert_blocked "bash -lc 'git push origin topic'"
assert_blocked 'SKIP_CONTAINER_VALIDATION=0 git push origin topic'
assert_blocked "SKIP_CONTAINER_VALIDATION=0 bash -lc 'git push origin topic'"
assert_blocked "SKIP_CONTAINER_VALIDATION=1 bash -lc 'SKIP_CONTAINER_VALIDATION=0 git push origin topic'"
assert_blocked "SKIP_CONTAINER_VALIDATION=1 bash -lc 'env -u SKIP_CONTAINER_VALIDATION git push origin topic'"
assert_blocked "SKIP_CONTAINER_VALIDATION=1 bash -lc 'env --unset SKIP_CONTAINER_VALIDATION git push origin topic'"
assert_blocked "SKIP_CONTAINER_VALIDATION=1 bash -lc 'env --unset=SKIP_CONTAINER_VALIDATION git push origin topic'"
assert_blocked "SKIP_CONTAINER_VALIDATION=1 bash -lc 'env -uSKIP_CONTAINER_VALIDATION git push origin topic'"
assert_blocked "SKIP_CONTAINER_VALIDATION=1 bash -lc 'unset SKIP_CONTAINER_VALIDATION; git push origin topic'"
assert_blocked 'SKIP_CONTAINER_VALIDATION=1 git push origin topic; git push origin other'

git -C "$tmpdir" init -q
git -C "$tmpdir" config user.email 'codex-test@example.invalid'
git -C "$tmpdir" config user.name 'Codex Gate Test'
git -C "$tmpdir" config commit.gpgsign false
printf 'base\n' > "$tmpdir/README"
git -C "$tmpdir" add README
git -C "$tmpdir" commit -qm 'base'
git -C "$tmpdir" branch -M main
git -C "$tmpdir" update-ref refs/remotes/origin/main HEAD
mkdir "$tmpdir/doc"
printf 'documentation-only change\n' > "$tmpdir/doc/example.rst"
printf 'readme-only change\n' >> "$tmpdir/README"
printf 'agent instructions\n' > "$tmpdir/AGENTS.md"
git -C "$tmpdir" add doc/example.rst README AGENTS.md
git -C "$tmpdir" commit -qm 'non-runtime change'
assert_allowed 'git push origin topic'

mkdir "$tmpdir/runtime"
printf 'int example;\n' > "$tmpdir/runtime/example.c"
git -C "$tmpdir" add runtime/example.c
git -C "$tmpdir" commit -qm 'runtime change'
assert_blocked 'git push origin topic'
