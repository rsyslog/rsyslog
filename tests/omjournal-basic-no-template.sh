#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Verify non-template PRIORITY contains severity alone and SYSLOG_FACILITY
# retains the separate facility. A unique message cookie selects the journal
# record, and clean shutdown drains rsyslog before its fields are checked.
# addd 2016-03-18 by RGerhards, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
. $srcdir/diag.sh require-journalctl
generate_conf
COOKIE=$RSYSLOG_DYNNAME
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
module(load="../plugins/omjournal/.libs/omjournal")
input(type="imtcp" address="127.0.0.1" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")

action(type="omjournal")
'
startup
tcpflood -m1 -M "\"<133>2011-03-01T11:22:12Z host tag msgh RsysLoG-TESTBENCH $COOKIE\""
shutdown_when_empty
wait_shutdown
# PRI 133 is facility 16 and severity 5. Journald exposes these as distinct
# SYSLOG_FACILITY and PRIORITY fields; exact-line checks reject a packed PRI.
journalctl --quiet --no-pager --output=json --output-fields=PRIORITY,SYSLOG_FACILITY,MESSAGE \
    --grep="$COOKIE" SYSLOG_IDENTIFIER=tag > "$RSYSLOG_DYNNAME.journal.out"
content_check "RsysLoG-TESTBENCH $COOKIE" "$RSYSLOG_DYNNAME.journal.out"
content_check '"PRIORITY":"5"' "$RSYSLOG_DYNNAME.journal.out"
content_check '"SYSLOG_FACILITY":"16"' "$RSYSLOG_DYNNAME.journal.out"
# if we reach this, we have at least not aborted
journalctl -r -t rsyslogd:  |grep "RsysLoG-TESTBENCH $COOKIE"
if [ $? -ne 1 ]; then
	echo "error: cookie $COOKIE not found. Head of journal:"
	journalctl -r -t rsyslogd: | head
	exit 1
fi
exit_test
