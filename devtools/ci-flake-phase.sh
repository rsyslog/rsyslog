#!/usr/bin/env bash
set -uo pipefail

usage() {
	echo "usage: $0 begin|end|run <name> <automake|custom|distcheck> [exit-code|-- command ...]" >&2
	exit 2
}

[ "$#" -ge 3 ] || usage
operation="$1"
name="$2"
phase_type="$3"
shift 3

case "$name" in
*[!A-Za-z0-9._-]*|'')
	echo "invalid flake phase name: $name" >&2
	exit 2
	;;
esac
case "$phase_type" in
automake|custom|distcheck) ;;
*) usage ;;
esac

evidence_root="${RSYSLOG_FLAKE_EVIDENCE_ROOT:-.ci/flake-evidence}"
phase_dir="$evidence_root/phases"
log_dir="$evidence_root/logs"
phase_file="$phase_dir/$name.phase"
mkdir -p "$phase_dir" "$log_dir"

begin_phase() {
	cat > "$phase_file" <<EOF
name=$name
type=$phase_type
status=interrupted
exit_code=
EOF
	printf 'RSYSLOG_FLAKE_PHASE_BEGIN name=%s type=%s\n' "$name" "$phase_type"
}

end_phase() {
	rc="$1"
	status=failure
	[ "$rc" -eq 0 ] && status=success
	cat > "$phase_file" <<EOF
name=$name
type=$phase_type
status=$status
exit_code=$rc
EOF
	printf 'RSYSLOG_FLAKE_PHASE_END name=%s type=%s status=%s exit_code=%s\n' \
		"$name" "$phase_type" "$status" "$rc"
}

case "$operation" in
begin)
	[ "$#" -eq 0 ] || usage
	begin_phase
	;;
end)
	[ "$#" -eq 1 ] || usage
	case "$1" in *[!0-9]*|'') usage ;; esac
	end_phase "$1"
	;;
run)
	[ "${1:-}" = "--" ] || usage
	shift
	[ "$#" -gt 0 ] || usage
	begin_phase
	set +e
	"$@" 2>&1 | tee "$log_dir/$name.log"
	rc="${PIPESTATUS[0]}"
	set -e
	end_phase "$rc"
	exit "$rc"
	;;
*) usage ;;
esac
