#!/bin/sh
# SPDX-License-Identifier: Apache-2.0
#
# Copyright 2026 Rainer Gerhards and Adiscon GmbH.
#
# Regression test for the Codex pre-push gate's explicit validation bypass.
# It proves that direct, env-prefixed, and shell-wrapped inline overrides are
# accepted, while ordinary and shell-wrapped pushes, false overrides, and
# mixed command lists still produce the hook's deny decision. Empty output is
# the allow oracle; the JSON permissionDecision=deny response is the block
# oracle. The shell-wrapped blocked case proves wrapper push recognition.
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
assert_blocked 'SKIP_CONTAINER_VALIDATION=1 git push origin topic; git push origin other'
