#!/bin/bash
# Verify that RELP configurations using the OpenSSL backend with the
# shared tls.prioritystring parameter produce an actionable diagnostic only
# when users pass a value that clearly looks like GnuTLS priority-string syntax.
# The oracle is the rsyslogd -N1 config-check output: no live RELP connection is
# needed. The warning must not fire for OpenSSL cipher-list syntax, because
# librelp also uses tls.prioritystring to pass cipher strings to OpenSSL.
# This file is part of the rsyslog project, released under ASL 2.0.
# shellcheck source=tests/diag.sh
. "${srcdir:=.}"/diag.sh init
require_relpEngineSetTLSLibByName
IMRELP_OPENSSL_VALID_LOG="${RSYSLOG_DYNNAME}.imrelp-openssl-valid.log"
OMRELP_GNUTLS_STYLE_LOG="${RSYSLOG_DYNNAME}.omrelp-gnutls-style.log"
OMRELP_OPENSSL_VALID_LOG="${RSYSLOG_DYNNAME}.omrelp-openssl-valid.log"

generate_conf
add_conf '
module(load="../plugins/imrelp/.libs/imrelp" tls.tlslib="openssl")
input(type="imrelp" port="0" tls="on" tls.prioritystring="NORMAL:+ANON-DH")

action(type="omfile" file="'"$RSYSLOG_OUT_LOG"'")
'
../tools/rsyslogd -C -N1 -M"$RSYSLOG_MODDIR" -f"${TESTCONF_NM}.conf" >"$RSYSLOG_OUT_LOG" 2>&1 || error_exit $?
content_check "imrelp: tls.prioritystring with tls.tlslib=\"openssl\" appears to use GnuTLS" "$RSYSLOG_OUT_LOG"
content_check "use OpenSSL cipher syntax or tls.tlscfgcmd" "$RSYSLOG_OUT_LOG"

generate_conf 2
add_conf '
module(load="../plugins/imrelp/.libs/imrelp" tls.tlslib="openssl")
input(type="imrelp" port="0" tls="on" tls.prioritystring="HIGH:!aNULL")

action(type="omfile" file="'"$IMRELP_OPENSSL_VALID_LOG"'")
' 2
../tools/rsyslogd -C -N1 -M"$RSYSLOG_MODDIR" -f"${TESTCONF_NM}2.conf" >"$IMRELP_OPENSSL_VALID_LOG" 2>&1 ||
    error_exit $?
check_not_present "appears to use GnuTLS" "$IMRELP_OPENSSL_VALID_LOG"

generate_conf 3
add_conf '
module(load="../plugins/omrelp/.libs/omrelp" tls.tlslib="openssl")

action(type="omrelp" target="127.0.0.1" port="'"$TCPFLOOD_PORT"'" tls="on"
       tls.prioritystring="NORMAL:+ANON-DH")
' 3
../tools/rsyslogd -C -N1 -M"$RSYSLOG_MODDIR" -f"${TESTCONF_NM}3.conf" >"$OMRELP_GNUTLS_STYLE_LOG" 2>&1 ||
    error_exit $?
content_check "omrelp: tls.prioritystring with tls.tlslib=\"openssl\" appears to use GnuTLS" "$OMRELP_GNUTLS_STYLE_LOG"
content_check "use OpenSSL cipher syntax or tls.tlscfgcmd" "$OMRELP_GNUTLS_STYLE_LOG"

generate_conf 4
add_conf '
module(load="../plugins/omrelp/.libs/omrelp" tls.tlslib="openssl")

action(type="omrelp" target="127.0.0.1" port="'"$TCPFLOOD_PORT"'" tls="on"
       tls.prioritystring="HIGH:!aNULL")
' 4
../tools/rsyslogd -C -N1 -M"$RSYSLOG_MODDIR" -f"${TESTCONF_NM}4.conf" >"$OMRELP_OPENSSL_VALID_LOG" 2>&1 ||
    error_exit $?
check_not_present "appears to use GnuTLS" "$OMRELP_OPENSSL_VALID_LOG"

exit_test
