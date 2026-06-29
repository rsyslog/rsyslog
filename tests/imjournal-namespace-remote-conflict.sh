#!/usr/bin/env bash
# Regression test for imjournal Namespace validation. Namespace uses the local
# systemd journal namespace API, while Remote uses the normal journal-open path
# for local plus remote journal files; the two settings must be rejected
# together during config validation. The oracle is rsyslogd -N1 failing before
# any live journal service is needed and emitting the exact conflict diagnostic.
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
require_plugin imjournal

generate_conf
add_conf '
module(load="../plugins/imjournal/.libs/imjournal"
       Namespace="testns"
       Remote="on")
'

../tools/rsyslogd -N1 -f"${TESTCONF_NM}.conf" -M"$RSYSLOG_MODDIR" >"${RSYSLOG_DYNNAME}.log" 2>&1
if [ $? -eq 0 ]; then
	echo "FAIL: expected config validation failure for imjournal Namespace with Remote"
	cat "${RSYSLOG_DYNNAME}.log"
	error_exit 1
fi

content_check "imjournal: Namespace and Remote cannot be enabled together" "${RSYSLOG_DYNNAME}.log"

exit_test
