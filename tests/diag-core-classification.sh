#!/usr/bin/env bash
# Verify core-dump helper classification stays conservative. Generic core files
# must not be accepted unless ownership can be attributed, while test-specific
# names remain a weak fallback for platforms where metadata extraction is not
# available. The external-collector override models systemd-coredump/apport
# policies that make local generic core scanning unreliable.
. ${srcdir:=.}/diag.sh init

# Keep synthetic core files out of the shared tests directory: parallel tests
# run broad core* cleanup during diag.sh init.
core_classification_dir="${PWD}/${RSYSLOG_DYNNAME}.core-classification.$$"
core_scan_log="${PWD}/${RSYSLOG_DYNNAME}.core-scan.log"

test_error_exit_handler() {
	rm -rf "$core_classification_dir"
}

rm -rf "$core_classification_dir"
mkdir "$core_classification_dir" || error_exit 1

touch "$core_classification_dir/core.foreign"
if core_candidate_owner_matches "$core_classification_dir/core.foreign" ../tools/rsyslogd 424242; then
	echo "FAIL: generic core without pid/executable ownership was accepted"
	error_exit 1
fi

touch "$core_classification_dir/core.rsyslogd.424242"
if ! core_candidate_owner_matches "$core_classification_dir/core.rsyslogd.424242" ../tools/rsyslogd 424242; then
	echo "FAIL: pid/executable filename hint was not accepted"
	error_exit 1
fi

touch "$core_classification_dir/core.${RSYSLOG_DYNNAME}.weak"
if ! core_candidate_owner_matches "$core_classification_dir/core.${RSYSLOG_DYNNAME}.weak" ../tools/rsyslogd ""; then
	echo "FAIL: test-specific core name fallback was not accepted"
	error_exit 1
fi

export RSTB_CORE_PATTERN_OVERRIDE='|/usr/lib/systemd/systemd-coredump %P %u %g %s %t %c %h'
if ! core_policy_uses_external_collector; then
	echo "FAIL: piped core_pattern was not recognized as external collector"
	error_exit 1
fi

core_dumps_found=0
oldpwd="$PWD"
cd "$core_classification_dir" || error_exit 1
analyze_owned_core_candidates ../../tools/rsyslogd "" rsyslogd > "$core_scan_log" 2>&1
cd "$oldpwd" || error_exit 1
if [ "$core_dumps_found" -ne 1 ]; then
	echo "FAIL: external-collector scan should analyze only the test-specific fallback core"
	cat "$core_scan_log"
	error_exit 1
fi
content_check "skipping generic local core scan because core_pattern uses an external collector" \
	"$core_scan_log"

rm -rf "$core_classification_dir"
exit_test
