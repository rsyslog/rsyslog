#!/bin/bash
# Regression test for loading the GnuTLS netstream driver by absolute path.
# The issue report used module(load="/custom/path/lmnsd_gtls.so"), which caused
# early driver initialization to dereference runConf before a config was active.
# Oracle: rsyslogd -C -N1 must accept the config without crashing.
# added 2026-06-23 by Codex
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init

gtls_module="$(cd .. && pwd)/runtime/.libs/lmnsd_gtls.so"
if [ ! -f "$gtls_module" ]; then
	echo "lmnsd_gtls module not built - skipping test"
	exit 77
fi

generate_conf
add_conf '
global(
	workDirectory="'$RSYSLOG_DYNNAME'.spool"
	debug.gnutls="1"
	defaultNetstreamDriver="gtls"
)
module(load="'$gtls_module'")
'

../tools/rsyslogd -C -N1 -f"${TESTCONF_NM}.conf" -M"$RSYSLOG_MODDIR" >"${RSYSLOG_DYNNAME}.log" 2>&1
if [ $? -ne 0 ]; then
	cat "${RSYSLOG_DYNNAME}.log"
	error_exit 1
fi

exit_test
