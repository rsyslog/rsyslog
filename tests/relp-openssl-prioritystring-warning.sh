#!/bin/bash
# Verify that RELP configurations using the OpenSSL backend with the
# shared tls.prioritystring parameter produce an actionable diagnostic for
# users who pass a GnuTLS priority-string value while OpenSSL is selected.
# The oracle is the rsyslogd -N1 config-check output: no live RELP connection is
# needed, and the warning must say OpenSSL interprets the value with OpenSSL
# cipher syntax while naming tls.tlscfgcmd as the broader policy alternative.
# This file is part of the rsyslog project, released under ASL 2.0.
# shellcheck source=tests/diag.sh
. "${srcdir:=.}"/diag.sh init
require_relpEngineSetTLSLibByName

generate_conf
add_conf '
module(load="../plugins/imrelp/.libs/imrelp" tls.tlslib="openssl")
input(type="imrelp" port="0" tls="on" tls.prioritystring="NORMAL:+ANON-DH")

action(type="omfile" file="'"$RSYSLOG_OUT_LOG"'")
'
../tools/rsyslogd -C -N1 -M"$RSYSLOG_MODDIR" -f"${TESTCONF_NM}.conf" >"$RSYSLOG_OUT_LOG" 2>&1 || error_exit $?
content_check "imrelp: tls.prioritystring with tls.tlslib=\"openssl\" is interpreted as an OpenSSL cipher string" "$RSYSLOG_OUT_LOG"
content_check "not as a GnuTLS priority string" "$RSYSLOG_OUT_LOG"
content_check "use OpenSSL cipher syntax or tls.tlscfgcmd" "$RSYSLOG_OUT_LOG"

generate_conf 2
add_conf '
module(load="../plugins/omrelp/.libs/omrelp" tls.tlslib="openssl")

action(type="omrelp" target="127.0.0.1" port="'"$TCPFLOOD_PORT"'" tls="on"
       tls.prioritystring="NORMAL:+ANON-DH")
' 2
../tools/rsyslogd -C -N1 -M"$RSYSLOG_MODDIR" -f"${TESTCONF_NM}2.conf" >"$RSYSLOG2_OUT_LOG" 2>&1 || error_exit $?
content_check "omrelp: tls.prioritystring with tls.tlslib=\"openssl\" is interpreted as an OpenSSL cipher string" "$RSYSLOG2_OUT_LOG"
content_check "not as a GnuTLS priority string" "$RSYSLOG2_OUT_LOG"
content_check "use OpenSSL cipher syntax or tls.tlscfgcmd" "$RSYSLOG2_OUT_LOG"

exit_test
