#!/bin/bash
# addd 2019-11-14 by alorbach, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
require_relpEngineSetTLSLibByName
export NUMMESSAGES=1000
export RSYSLOG_DEBUG="debug nologfuncflow noprintmutexaction nostdout"
export RSYSLOG_DEBUGLOG="$RSYSLOG_DYNNAME.receiver.debuglog"
generate_conf
add_conf '
module(	load="../plugins/imrelp/.libs/imrelp" 
	tls.tlslib="openssl")
input(type="imrelp" port="'$TCPFLOOD_PORT'" tls="on"
		tls.cacert="'$srcdir'/tls-certs/ca.pem"
		tls.mycert="'$srcdir'/tls-certs/cert.pem"
		tls.myprivkey="'$srcdir'/tls-certs/key.pem"
		tls.authmode="certvalid"
		tls.permittedpeer="rsyslog"
		tls.tlscfgcmd="Protocol=ALL,-SSLv2,-SSLv3,-TLSv1,-TLSv1.2
CipherString=ECDHE-RSA-AES256-GCM-SHA384
Protocol=ALL,-SSLv2,-SSLv3,-TLSv1,-TLSv1.2,-TLSv1.3
MinProtocol=TLSv1.2
MaxProtocol=TLSv1.2")

template(name="outfmt" type="string" string="%msg:F,58:2%\n")
:msg, contains, "msgnum:" action(type="omfile" template="outfmt"
			         file=`echo $RSYSLOG_OUT_LOG`)
'
startup

export TCPFLOOD_EXTRA_OPTS='-k "Protocol=ALL,-SSLv2,-SSLv3,-TLSv1.1,-TLSv1.2
CipherString=DHE-RSA-AES256-SHA
Protocol=ALL,-SSLv2,-SSLv3,-TLSv1.1,-TLSv1.2,-TLSv1.3
MinProtocol=TLSv1.1
MaxProtocol=TLSv1.1"'
tcpflood --check-only -u "openssl" -Trelp-tls -acertvalid -p$TCPFLOOD_PORT -m$NUMMESSAGES -x "$srcdir/tls-certs/ca.pem" -z "$srcdir/tls-certs/key.pem" -Z "$srcdir/tls-certs/cert.pem" -Ersyslog 2> ${RSYSLOG_DYNNAME}.tcpflood

shutdown_when_empty
wait_shutdown

content_check --check-only "relpTcpTLSSetPrio_gtls" ${RSYSLOG_DEBUGLOG}
ret=$?
if [ $ret == 0 ]; then
	echo "SKIP: LIBRELP was build without OPENSSL Support"
	skip_test
fi 

content_check --check-only "OpenSSL Version too old" ${RSYSLOG_DEBUGLOG}
ret=$?
if [ $ret == 0 ]; then
	echo "SKIP: OpenSSL Version too old"
	skip_test
else
	# Check for a failed session - possible ecodes are 10031 and 10040
	content_check "librelp: generic error: ecode" $RSYSLOG_DEBUGLOG
fi

exit_test