#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# added 2011-02-28 by Rgerhards
# This file is part of the rsyslog project, released under ASL 2.0
# Verify a successful OpenSSL TLS handshake does not emit the obsolete
# post-handshake "no shared curve" diagnostic. OpenSSL versions with SSL_CONF
# MinProtocol/MaxProtocol support are pinned to TLS 1.2; older OpenSSL and
# LibreSSL use their defaults but retain the same deterministic query oracle.
# A probe first calls the exact shared-curve API under LD_PRELOAD and requires
# query-hit evidence, so the test cannot pass because the interposer was inert.
# The complete message sequence proves TLS succeeded; synchronized shutdown
# flushes internal messages before checking.
. ${srcdir:=.}/diag.sh init
skip_platform "AIX" "test requires shared-library interposition"
skip_platform "Darwin" "test uses ELF LD_PRELOAD interposition"
skip_ASAN "LD_PRELOAD conflicts with ASan runtime load order"

export NUMMESSAGES=50000
export QUEUE_EMPTY_CHECK_FUNC=wait_seq_check
export RSYSLOG_PRELOAD=.libs/liboverride_ossl_shared_curve.so
export RSYSLOG_TEST_SSL_SHARED_CURVE_PRELOAD_MARKER="$PWD/$RSYSLOG_DYNNAME.shared-curve-preload"
export RSYSLOG_TEST_SSL_SHARED_CURVE_QUERY_MARKER="$PWD/$RSYSLOG_DYNNAME.shared-curve-query-hit"

if ./openssl_shared_curve_probe --supports-tls12-pinning; then
	OSSL_TLS12_SERVER_CFG='gnutlsPriorityString="MaxProtocol=TLSv1.2"'
	OSSL_TLS12_CLIENT_ARGS=(-k 'MinProtocol=TLSv1.2')
	OSSL_TLS12_PINNING=1
else
	# SSL_CONF MinProtocol/MaxProtocol is unavailable on OpenSSL 1.0.2 and
	# LibreSSL. Keep the successful TLS and forced-query coverage on those builds.
	echo "INFO: OpenSSL SSL_CONF TLS-version commands unavailable; using library-default protocol negotiation"
	OSSL_TLS12_SERVER_CFG=''
	OSSL_TLS12_CLIENT_ARGS=()
	OSSL_TLS12_PINNING=0
fi

probe_loaded_marker="$PWD/$RSYSLOG_DYNNAME.shared-curve-probe-loaded"
probe_query_marker="$PWD/$RSYSLOG_DYNNAME.shared-curve-probe-query-hit"
rm -f "$RSYSLOG_TEST_SSL_SHARED_CURVE_PRELOAD_MARKER" "$RSYSLOG_TEST_SSL_SHARED_CURVE_QUERY_MARKER" \
	"$probe_loaded_marker" "$probe_query_marker"
RSYSLOG_TEST_SSL_SHARED_CURVE_PRELOAD_MARKER="$probe_loaded_marker" \
	RSYSLOG_TEST_SSL_SHARED_CURVE_QUERY_MARKER="$probe_query_marker" \
	LD_PRELOAD="$RSYSLOG_PRELOAD" ./openssl_shared_curve_probe || error_exit 1
if [ ! -s "$probe_loaded_marker" ] || [ ! -s "$probe_query_marker" ]; then
	echo "FAIL: OpenSSL shared-curve query interposer did not record a query hit"
	error_exit 1
fi
rm -f "$probe_loaded_marker" "$probe_query_marker"

generate_conf
: > "$RSYSLOG_DYNNAME.internal.log"
add_conf '
global(
    processInternalMessages="on"
    internalmsg.severity="info"
#	debug.whitelist="on"
#	debug.files=["net_ossl.c", "nsd_ossl.c", "nsd_ptcp.c", "tcpsrv.c", "nsdsel_ossl.c", "nsdpoll_ptcp.c", "dnscache.c"]
)

module(load="../plugins/imtcp/.libs/imtcp"
	StreamDriver.Name="ossl"
	StreamDriver.Mode="1"
	StreamDriver.AuthMode="anon"
'$OSSL_TLS12_SERVER_CFG' )

input(type="imtcp" address="127.0.0.1" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port"
        streamDriver.CAFile="'$srcdir'/tls-certs/ca.pem"
	streamDriver.CertFile="'$srcdir'/tls-certs/cert.pem"
	streamDriver.KeyFile="'$srcdir'/tls-certs/key.pem")


template(name="outfmt" type="string" string="%msg:F,58:2%\n")
:msg, contains, "msgnum:" action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
:msg, contains, "no shared curve" action(type="omfile" file="'$RSYSLOG_DYNNAME'.internal.log")
:msg, contains, "Failed to add OpenSSL command" action(type="omfile" file="'$RSYSLOG_DYNNAME'.internal.log")
:msg, contains, "does not support SSL_CONF_cmd" action(type="omfile" file="'$RSYSLOG_DYNNAME'.internal.log")
'
startup
if [ ! -s "$RSYSLOG_TEST_SSL_SHARED_CURVE_PRELOAD_MARKER" ]; then
	echo "FAIL: OpenSSL shared-curve interposer was not loaded"
	error_exit 1
fi
tcpflood -p$TCPFLOOD_PORT -m$NUMMESSAGES -Ttls "${OSSL_TLS12_CLIENT_ARGS[@]}" \
	-x$srcdir/tls-certs/ca.pem -Z$srcdir/tls-certs/cert.pem -z$srcdir/tls-certs/key.pem \
	> "$RSYSLOG_DYNNAME.tcpflood.log" || error_exit 1
shutdown_when_empty
wait_shutdown
seq_check
check_not_present "no shared curve" "$RSYSLOG_DYNNAME.internal.log"
if [ "$OSSL_TLS12_PINNING" -eq 1 ]; then
	content_check "Successfully added Command MinProtocol:TLSv1.2" "$RSYSLOG_DYNNAME.tcpflood.log"
	check_not_present "Failed to add OpenSSL command" "$RSYSLOG_DYNNAME.internal.log"
	check_not_present "does not support SSL_CONF_cmd" "$RSYSLOG_DYNNAME.internal.log"
fi
exit_test
