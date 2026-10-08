#!/bin/bash
# Regression test for https://github.com/rsyslog/rsyslog/issues/7547, reported
# by LuciyVI. Legacy ompipe paths at 511 bytes, the former 512-byte overflow
# boundary, and MAXFNAME-1 must parse without an invalid write. The oracle is
# successful -N1 config validation; the Valgrind wrapper additionally turns an
# invalid access into a failing process status without requiring a FIFO.
. ${srcdir:=.}/diag.sh init

build_path() {
	local target_length="$1"
	local component
	local remaining
	local path=/tmp

	printf -v component '%*s' 200 ''
	component=${component// /x}
	while (( ${#path} + ${#component} + 1 <= target_length )); do
		path+="/$component"
	done

	remaining=$((target_length - ${#path}))
	if (( remaining > 0 )); then
		printf -v component '%*s' "$((remaining - 1))" ''
		component=${component// /y}
		path+="/$component"
	fi

	printf '%s' "$path"
}

path_511="$(build_path 511)"
path_512="$(build_path 512)"
path_4095="$(build_path 4095)"

if (( ${#path_511} != 511 || ${#path_512} != 512 || ${#path_4095} != 4095 )); then
	error_exit 1 "test setup generated an incorrect ompipe path length"
fi

generate_conf
add_conf "
*.* |${path_511}
*.* |${path_512}
*.* |${path_4095}
"

log_file="${RSYSLOG_DYNNAME}.config-check.log"
rsyslogd_command=(../tools/rsyslogd -C -N1 -M"$RSYSLOG_MODDIR" -f"${TESTCONF_NM}.conf")
if [ "${USE_VALGRIND:-}" = "YES" ]; then
	rsyslogd_command=(valgrind --error-exitcode=10 --leak-check=no --malloc-fill=ff --free-fill=fe \
		--suppressions="$srcdir/known_issues.supp" "${rsyslogd_command[@]}")
fi

"${rsyslogd_command[@]}" >"$log_file" 2>&1
rsyslogd_status=$?
if [ "$rsyslogd_status" -ne 0 ]; then
	cat "$log_file"
	error_exit "$rsyslogd_status" "legacy ompipe path config validation failed"
fi
content_check "End of config validation run" "$log_file"
exit_test
