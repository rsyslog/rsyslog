#!/bin/sh
# Regression test for the Codex pre-push gate's explicit validation bypass.
# It proves that direct, env-prefixed, and shell-wrapped inline overrides are
# accepted, while ordinary pushes, false overrides, and mixed command lists
# still produce the hook's deny decision. Empty output is the allow oracle;
# the JSON permissionDecision=deny response is the block oracle.
set -eu

tmpdir="$(mktemp -d "${TMPDIR:-/tmp}/rsyslog-codex-push-gate.XXXXXX")"
trap 'rm -rf "$tmpdir"' EXIT HUP INT TERM

mkdir -p "$tmpdir/.codex"
cp "${srcdir:-.}/../.codex/pre_push_container_gate.sh" "$tmpdir/.codex/"
gate="$tmpdir/.codex/pre_push_container_gate.sh"

run_gate() {
	command_text="$1"
	printf '%s\n' \
		'{"hook_event_name":"PreToolUse","tool_name":"Bash","tool_input":{"command":"'"$command_text"'"}}' |
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

assert_blocked 'git push origin topic'
assert_blocked 'SKIP_CONTAINER_VALIDATION=0 git push origin topic'
assert_blocked 'SKIP_CONTAINER_VALIDATION=1 git push origin topic; git push origin other'
