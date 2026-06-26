#!/bin/bash
# Regression test for tcpflood's GnuTLS priority-string option. The oracle is
# that an invalid priority string is rejected during TLS initialization before a
# network connection is attempted, proving the option is parsed and handed to
# GnuTLS directly instead of being ignored.
# This file is part of the rsyslog project, released under ASL 2.0
unset RSYSLOG_DYNNAME RSYSLOG_OUT_LOG TESTCONF_NM
. ${srcdir:=.}/diag.sh init

./tcpflood -Ttls -g INVALID-GNUTLS-PRIORITY \
	-z "$srcdir/tls-certs/key.pem" \
	-Z "$srcdir/tls-certs/cert.pem" \
	>"$RSYSLOG_OUT_LOG" 2>&1
if [ $? -eq 0 ]; then
	echo "FAIL: tcpflood accepted an invalid GnuTLS priority string"
	cat "$RSYSLOG_OUT_LOG"
	exit 1
fi

if content_check --check-only "-g requires tcpflood to use the GnuTLS TLS implementation"; then
	echo "SKIP: tcpflood is not using the GnuTLS TLS implementation"
	skip_test
fi

content_check "tcpflood: invalid GnuTLS priority string 'INVALID-GNUTLS-PRIORITY'"

exit_test
