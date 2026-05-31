#!/bin/bash
# Verify that RELP configurations using the OpenSSL backend with the
# shared tls.prioritystring parameter produce an actionable diagnostic only when
# users pass a value that clearly looks like the other backend's syntax.
# The oracle is the rsyslogd -N1 config-check output: no live RELP connection is
# needed. The warning must not fire for matching backend syntax, because librelp
# uses this single parameter for both GnuTLS priority strings and OpenSSL cipher
# lists.
# This file is part of the rsyslog project, released under ASL 2.0.
# shellcheck source=tests/diag.sh
. "${srcdir:=.}"/diag.sh init
require_relpEngineSetTLSLibByName
IMRELP_OPENSSL_VALID_LOG="${RSYSLOG_DYNNAME}.imrelp-openssl-valid.log"
IMRELP_GNUTLS_OPENSSL_STYLE_LOG="${RSYSLOG_DYNNAME}.imrelp-gnutls-openssl-style.log"
IMRELP_GNUTLS_VALID_LOG="${RSYSLOG_DYNNAME}.imrelp-gnutls-valid.log"
OMRELP_GNUTLS_STYLE_LOG="${RSYSLOG_DYNNAME}.omrelp-gnutls-style.log"
OMRELP_OPENSSL_VALID_LOG="${RSYSLOG_DYNNAME}.omrelp-openssl-valid.log"
OMRELP_GNUTLS_OPENSSL_STYLE_LOG="${RSYSLOG_DYNNAME}.omrelp-gnutls-openssl-style.log"
OMRELP_GNUTLS_VALID_LOG="${RSYSLOG_DYNNAME}.omrelp-gnutls-valid.log"

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
module(load="../plugins/imrelp/.libs/imrelp" tls.tlslib="gnutls")
input(type="imrelp" port="0" tls="on" tls.prioritystring="HIGH:!aNULL")

action(type="omfile" file="'"$IMRELP_GNUTLS_OPENSSL_STYLE_LOG"'")
' 3
../tools/rsyslogd -C -N1 -M"$RSYSLOG_MODDIR" -f"${TESTCONF_NM}3.conf" >"$IMRELP_GNUTLS_OPENSSL_STYLE_LOG" 2>&1 ||
    error_exit $?
content_check "imrelp: tls.prioritystring with the GnuTLS RELP backend appears to use OpenSSL" \
    "$IMRELP_GNUTLS_OPENSSL_STYLE_LOG"
content_check "use GnuTLS priority-string syntax or switch tls.tlslib to \"openssl\"" \
    "$IMRELP_GNUTLS_OPENSSL_STYLE_LOG"

generate_conf 4
add_conf '
module(load="../plugins/imrelp/.libs/imrelp" tls.tlslib="gnutls")
input(type="imrelp" port="0" tls="on" tls.prioritystring="NORMAL:+ANON-DH")

action(type="omfile" file="'"$IMRELP_GNUTLS_VALID_LOG"'")
' 4
../tools/rsyslogd -C -N1 -M"$RSYSLOG_MODDIR" -f"${TESTCONF_NM}4.conf" >"$IMRELP_GNUTLS_VALID_LOG" 2>&1 ||
    error_exit $?
check_not_present "appears to use OpenSSL" "$IMRELP_GNUTLS_VALID_LOG"

generate_conf 5
add_conf '
module(load="../plugins/omrelp/.libs/omrelp" tls.tlslib="openssl")

action(type="omrelp" target="127.0.0.1" port="'"$TCPFLOOD_PORT"'" tls="on"
       tls.prioritystring="NORMAL:+ANON-DH")
' 5
../tools/rsyslogd -C -N1 -M"$RSYSLOG_MODDIR" -f"${TESTCONF_NM}5.conf" >"$OMRELP_GNUTLS_STYLE_LOG" 2>&1 ||
    error_exit $?
content_check "omrelp: tls.prioritystring with tls.tlslib=\"openssl\" appears to use GnuTLS" "$OMRELP_GNUTLS_STYLE_LOG"
content_check "use OpenSSL cipher syntax or tls.tlscfgcmd" "$OMRELP_GNUTLS_STYLE_LOG"

generate_conf 6
add_conf '
module(load="../plugins/omrelp/.libs/omrelp" tls.tlslib="openssl")

action(type="omrelp" target="127.0.0.1" port="'"$TCPFLOOD_PORT"'" tls="on"
       tls.prioritystring="HIGH:!aNULL")
' 6
../tools/rsyslogd -C -N1 -M"$RSYSLOG_MODDIR" -f"${TESTCONF_NM}6.conf" >"$OMRELP_OPENSSL_VALID_LOG" 2>&1 ||
    error_exit $?
check_not_present "appears to use GnuTLS" "$OMRELP_OPENSSL_VALID_LOG"

generate_conf 7
add_conf '
module(load="../plugins/omrelp/.libs/omrelp" tls.tlslib="gnutls")

action(type="omrelp" target="127.0.0.1" port="'"$TCPFLOOD_PORT"'" tls="on"
       tls.prioritystring="HIGH:!aNULL")
' 7
../tools/rsyslogd -C -N1 -M"$RSYSLOG_MODDIR" -f"${TESTCONF_NM}7.conf" >"$OMRELP_GNUTLS_OPENSSL_STYLE_LOG" 2>&1 ||
    error_exit $?
content_check "omrelp: tls.prioritystring with the GnuTLS RELP backend appears to use OpenSSL" \
    "$OMRELP_GNUTLS_OPENSSL_STYLE_LOG"
content_check "use GnuTLS priority-string syntax or switch tls.tlslib to \"openssl\"" \
    "$OMRELP_GNUTLS_OPENSSL_STYLE_LOG"

generate_conf 8
add_conf '
module(load="../plugins/omrelp/.libs/omrelp" tls.tlslib="gnutls")

action(type="omrelp" target="127.0.0.1" port="'"$TCPFLOOD_PORT"'" tls="on"
       tls.prioritystring="NORMAL:+ANON-DH")
' 8
../tools/rsyslogd -C -N1 -M"$RSYSLOG_MODDIR" -f"${TESTCONF_NM}8.conf" >"$OMRELP_GNUTLS_VALID_LOG" 2>&1 ||
    error_exit $?
check_not_present "appears to use OpenSSL" "$OMRELP_GNUTLS_VALID_LOG"

exit_test
