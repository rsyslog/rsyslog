#!/bin/bash
# Regression test for backtick cat expansion from regular pseudo-files such
# as /proc. The positive oracle is config validation success: /proc/self/comm
# commonly reports st_size 0, so failure means read_file still treats
# pseudo-file size metadata as the exact number of readable bytes. The negative
# oracle rejects special files such as /dev/zero quickly, proving the fallback
# EOF read path cannot hang or grow without bound on non-regular inputs. Failed
# reads are reported as parser errors and config validation fails in expression
# context.
. ${srcdir:=.}/diag.sh init

if [ ! -r /proc/self/comm ]; then
    echo "SKIP: /proc/self/comm is not readable"
    exit 77
fi

generate_conf
add_conf '
if `cat /proc/self/comm` != "" then
    stop
'

../tools/rsyslogd -C -N1 -M"${RSYSLOG_MODDIR}" -f"${TESTCONF_NM}.conf" >"${RSYSLOG_DYNNAME}.log" 2>&1
if [ $? -ne 0 ]; then
    echo "FAIL: expected config validation with /proc backtick cat to succeed"
    cat "${RSYSLOG_DYNNAME}.log"
    error_exit 1
fi

if grep -F 'file could not be accessed' "${RSYSLOG_DYNNAME}.log" >/dev/null; then
    echo "FAIL: /proc backtick cat reported a file access error"
    cat "${RSYSLOG_DYNNAME}.log"
    error_exit 1
fi

if [ -r /dev/zero ]; then
	generate_conf 2
	add_conf '
if `cat /dev/zero` == "" then
    stop
' 2

	timeout 5s ../tools/rsyslogd -C -N1 -M"${RSYSLOG_MODDIR}" \
		-f"${TESTCONF_NM}2.conf" >"${RSYSLOG_DYNNAME}.devzero.log" 2>&1
	ret=$?
	if [ $ret -eq 124 ]; then
		echo "FAIL: /dev/zero backtick cat validation timed out"
		cat "${RSYSLOG_DYNNAME}.devzero.log"
		error_exit 1
	fi
	if [ $ret -eq 0 ]; then
		echo "FAIL: expected config validation with /dev/zero backtick cat to fail"
		cat "${RSYSLOG_DYNNAME}.devzero.log"
		error_exit 1
	fi
	if ! grep -F 'file could not be accessed' "${RSYSLOG_DYNNAME}.devzero.log" >/dev/null; then
		echo "FAIL: /dev/zero backtick cat did not report a file access error"
		cat "${RSYSLOG_DYNNAME}.devzero.log"
		error_exit 1
	fi
fi
exit_test
