#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 Rainer Gerhards and Adiscon GmbH.
# Verify that imklog emits its own messages with the configured
# internalMsgFacility. LogPath points to a directory: it can be opened,
# but the read check after startup fails (EISDIR on Linux), so imklog logs
# an error about the kernel log through its internal-message path.
# Oracle: that error message is logged as local0.err. If the system does
# not reject the read on a directory, the error never appears; the test
# then fails with an explicit message after 20 s instead of running into
# the generic testbench timeout.
. ${srcdir:=.}/diag.sh init
require_plugin imklog
mkdir -p "$RSYSLOG_DYNNAME.klogdir"
generate_conf
if [ "${IMKLOG_INTERNALMSGFACILITY_YAML:-}" = YES ]; then
	require_yaml_support
	cat > "$RSYSLOG_DYNNAME.imklog.yaml" <<EOF
modules:
  - load: "../plugins/imklog/.libs/imklog"
    logpath: "$RSYSLOG_DYNNAME.klogdir"
    internalMsgFacility: local0
EOF
	add_conf 'include(file="'${RSYSLOG_DYNNAME}'.imklog.yaml")'
else
	add_conf '
module(load="../plugins/imklog/.libs/imklog" logpath="'$RSYSLOG_DYNNAME.klogdir'"
       internalMsgFacility="local0")
'
fi
add_conf '
template(name="outfmt" type="string" string="%syslogfacility-text%.%syslogseverity-text%\n")
:msg, contains, "imklog: cannot open kernel log" action(type="omfile" template="outfmt" file="'$RSYSLOG_OUT_LOG'")
'
startup
waited=0
until grep -q "imklog: cannot open kernel log" "$RSYSLOG_DYNNAME.started" 2>/dev/null; do
	if [ "$waited" -ge 200 ]; then
		echo "FAIL: imklog did not report the directory LogPath as unreadable"
		echo "(reading a directory did not fail on this system?)"
		error_exit 1
	fi
	$TESTTOOL_DIR/msleep 100
	waited=$((waited + 1))
done
wait_file_lines "$RSYSLOG_OUT_LOG" 1
shutdown_when_empty
wait_shutdown
export EXPECTED="local0.err"
cmp_exact
exit_test
