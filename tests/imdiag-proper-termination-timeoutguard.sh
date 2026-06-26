#!/usr/bin/env bash
# Verify timeoutGuard writes the imdiag proper termination marker before aborting.
export TEST_MAX_RUNTIME=2
. ${srcdir:=.}/diag.sh init

if [ -n "$USE_VALGRIND" ]; then
	printf 'timeoutGuard validation intentionally aborts rsyslogd; skipping valgrind mode\n'
	skip_test
fi

# diag.sh raises the core limit for crash diagnostics. This test intentionally
# aborts rsyslogd, so disable core files after init to avoid polluting CI hosts.
ulimit -c 0 || true

generate_conf
export RS_REDIR=">$RSYSLOG_DYNNAME.rsyslog.log 2>&1"
startup_common "" ""
bash -c "LD_PRELOAD=$RSYSLOG_PRELOAD ../tools/rsyslogd -C -n -i$RSYSLOG_PIDBASE$instance.pid -M\"$RSYSLOG_MODDIR\" -f$CONF_FILE $RS_REDIR" >/dev/null 2>&1 &
rsyslog_job_pid=$!
wait_startup "$instance"
reassign_ports

marker=$(get_proper_termination_file)
wait_file_exists "$marker"
wait "$rsyslog_job_pid" >/dev/null 2>&1

print_proper_termination_file
if proper_termination_file_is_valid; then
	printf 'timeoutGuard validation: abort marker unexpectedly passed clean-shutdown validation\n'
	error_exit 1
fi

grep -qx 'status=error' "$marker" || error_exit 1
grep -qx 'reason=timeoutGuard' "$marker" || error_exit 1
grep -qx 'phase=timeoutguard-abort' "$marker" || error_exit 1
grep -Eq '^queue\.overall\.size=[0-9]+$' "$marker" || error_exit 1

exit_test
