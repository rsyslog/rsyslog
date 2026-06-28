#!/usr/bin/env bash
# Verify core-dump helper classification stays conservative. Generic core files
# must not be accepted unless ownership can be attributed, while test-specific
# names remain a weak fallback for platforms where metadata extraction is not
# available. The external-collector override models systemd-coredump/apport
# policies that make local generic core scanning unreliable.
. ${srcdir:=.}/diag.sh init

test_error_exit_handler() {
	rm -f core.foreign core.rsyslogd.424242 core.${RSYSLOG_DYNNAME}.weak
}

touch core.foreign
if core_candidate_owner_matches core.foreign ../tools/rsyslogd 424242; then
	echo "FAIL: generic core without pid/executable ownership was accepted"
	error_exit 1
fi

touch core.rsyslogd.424242
if ! core_candidate_owner_matches core.rsyslogd.424242 ../tools/rsyslogd 424242; then
	echo "FAIL: pid/executable filename hint was not accepted"
	error_exit 1
fi

touch core.${RSYSLOG_DYNNAME}.weak
if ! core_candidate_owner_matches core.${RSYSLOG_DYNNAME}.weak ../tools/rsyslogd ""; then
	echo "FAIL: test-specific core name fallback was not accepted"
	error_exit 1
fi

export RSTB_CORE_PATTERN_OVERRIDE='|/usr/lib/systemd/systemd-coredump %P %u %g %s %t %c %h'
if ! core_policy_uses_external_collector; then
	echo "FAIL: piped core_pattern was not recognized as external collector"
	error_exit 1
fi

core_dumps_found=0
analyze_owned_core_candidates ../tools/rsyslogd "" rsyslogd > "${RSYSLOG_DYNNAME}.core-scan.log" 2>&1
if [ "$core_dumps_found" -ne 1 ]; then
	echo "FAIL: external-collector scan should analyze only the test-specific fallback core"
	cat "${RSYSLOG_DYNNAME}.core-scan.log"
	error_exit 1
fi
content_check "skipping generic local core scan because core_pattern uses an external collector" \
	"${RSYSLOG_DYNNAME}.core-scan.log"

rm -f core.foreign core.rsyslogd.424242 core.${RSYSLOG_DYNNAME}.weak
exit_test
