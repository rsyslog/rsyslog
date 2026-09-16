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
# It also proves that validation follows a literal `git -C` target or supplied
# tool/session workdir instead of the hook script's checkout. A validated
# runtime commit followed only by docs is allowed, while a later runtime change
# and a second stale target block. Empty output is the allow oracle; the JSON
# permissionDecision=deny response is the block oracle. Directory-changing
# shells and separate Git directory/worktree options must block because they
# cannot identify one safe worktree. This is intentionally standalone rather
# than a diag.sh scenario: it exercises only hook command parsing and needs
# neither rsyslogd nor testbench helpers.
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
	tool_workdir="${2:-}"
	session_cwd="${3:-$tmpdir}"
	python3 -c '
import json
import sys

tool_input = {"command": sys.argv[1]}
if sys.argv[2]:
    tool_input["workdir"] = sys.argv[2]

print(json.dumps({
    "hook_event_name": "PreToolUse",
    "tool_name": "Bash",
    "tool_input": tool_input,
    "cwd": sys.argv[3],
}))
' "$command_text" "$tool_workdir" "$session_cwd" |
		"$gate"
}

assert_allowed() {
	command_text="$1"
	output="$(run_gate "$command_text" "${2:-}" "${3:-}")"
	if [ -n "$output" ]; then
		printf 'expected push command to be allowed: %s\n%s\n' "$command_text" "$output" >&2
		exit 1
	fi
}

assert_blocked() {
	command_text="$1"
	output="$(run_gate "$command_text" "${2:-}" "${3:-}")"
	case "$output" in
		*'"permissionDecision": "deny"'*) ;;
		*)
			printf 'expected push command to be blocked: %s\n%s\n' "$command_text" "$output" >&2
			exit 1
			;;
	esac
	printf '%s' "$output" | python3 -c '
import json
import sys

decision = json.load(sys.stdin)
assert decision["hookSpecificOutput"]["permissionDecision"] == "deny"
'
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
assert_allowed "git --git-dir '$tmpdir/not-a-repo' status"
assert_allowed "git --work-tree '$tmpdir/not-a-worktree' log"

assert_runtime_delta_blocked() {
	path="$1"
	content="$2"
	mkdir -p "$tmpdir/$(dirname "$path")"
	printf '%s\n' "$content" > "$tmpdir/$path"
	git -C "$tmpdir" add "$path"
	git -C "$tmpdir" commit -qm "add $path"
	assert_blocked 'git push origin topic'
	git -C "$tmpdir" reset --hard -q HEAD~1
}

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

assert_runtime_delta_blocked 'runtime/example.c' 'int example;'
assert_runtime_delta_blocked 'Dockerfile' 'FROM scratch'
assert_runtime_delta_blocked 'MODULE_METADATA.yaml' 'module: example'
assert_runtime_delta_blocked 'devtools/example.py' 'print("example")'

create_target() {
	target="$1"
	marker_kind="$2"
	mkdir -p "$target/.codex"
	git -C "$target" init -q
	git -C "$target" config user.email 'codex-test@example.invalid'
	git -C "$target" config user.name 'Codex Gate Test'
	git -C "$target" config commit.gpgsign false
	printf 'base\n' > "$target/README"
	git -C "$target" add README
	git -C "$target" commit -qm 'base'
	base_commit="$(git -C "$target" rev-parse HEAD)"
	git -C "$target" branch -M main
	git -C "$target" update-ref refs/remotes/origin/main HEAD
	mkdir "$target/runtime"
	printf 'int runtime_change;\n' > "$target/runtime/example.c"
	git -C "$target" add runtime/example.c
	git -C "$target" commit -qm 'runtime change'
	runtime_commit="$(git -C "$target" rev-parse HEAD)"
	mkdir "$target/doc"
	printf 'documentation-only tail\n' > "$target/doc/example.rst"
	git -C "$target" add doc/example.rst
	git -C "$target" commit -qm 'documentation tail'
	case "$marker_kind" in
	valid) printf '%s\n' "$runtime_commit" > "$target/.codex/container_validated.marker" ;;
	stale) printf '%s\n' "$base_commit" > "$target/.codex/container_validated.marker" ;;
	esac
}

target_valid="$tmpdir/target valid"
target_stale="$tmpdir/target stale"
create_target "$target_valid" valid
create_target "$target_stale" stale

# The session checkout now has a runtime delta and an old marker. These checks
# prove that explicit target selection uses each target's state instead.
printf '%s\n' "$(git -C "$tmpdir" rev-parse HEAD~1)" > "$tmpdir/.codex/container_validated.marker"
mkdir -p "$tmpdir/runtime"
printf 'int session_stale;\n' > "$tmpdir/runtime/session-stale.c"
git -C "$tmpdir" add runtime/session-stale.c
git -C "$tmpdir" commit -qm 'session runtime change'

assert_allowed "git -C '$target_valid' push origin topic"
assert_allowed "git -C '$tmpdir' -C 'target valid' push origin topic"
assert_allowed "bash -lc \"git -C '$target_valid' push origin topic\""
assert_allowed 'git push origin topic' "$target_valid" "$tmpdir"
assert_allowed 'git push origin topic' '' "$target_valid"
assert_blocked 'git push origin topic'
assert_blocked "git -C '$target_stale' push origin topic"
assert_blocked "git -C '$target_valid' push origin topic; git -C '$target_stale' push origin topic"
assert_blocked "bash -lc \"cd '$target_valid'; git push origin topic\""
assert_allowed "bash -lc \"cd '$tmpdir'; git -C '$target_valid' push origin topic\""
assert_blocked "bash -lc \"cd '$tmpdir'; git -C 'target valid' push origin topic\""
assert_blocked "git --git-dir '$target_valid/.git' push origin topic"
assert_blocked "git --work-tree '$target_valid' push origin topic"
assert_blocked "git -C '$tmpdir/nonexistent \"quoted\" target' push origin topic"

printf 'int later_runtime_change;\n' > "$target_valid/runtime/later.c"
git -C "$target_valid" add runtime/later.c
git -C "$target_valid" commit -qm 'later runtime change'
assert_blocked "git -C '$target_valid' push origin topic"
