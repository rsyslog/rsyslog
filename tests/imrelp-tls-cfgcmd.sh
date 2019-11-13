#!/bin/bash
# addd 2019-11-14 by alorbach, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
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
		tls.tlscfgcmd="Protocol=ALL,-SSLv2,-SSLv3,-TLSv1,-TLSv1.2")

template(name="outfmt" type="string" string="%msg:F,58:2%\n")
:msg, contains, "msgnum:" action(type="omfile" template="outfmt"
			         file=`echo $RSYSLOG_OUT_LOG`)
'
startup

tcpflood --check-only -k "Protocol=-ALL,TLSv1.2" -u "openssl" -Trelp-tls -acertvalid -p$TCPFLOOD_PORT -m$NUMMESSAGES -x "$srcdir/tls-certs/ca.pem" -z "$srcdir/tls-certs/key.pem" -Z "$srcdir/tls-certs/cert.pem" -Ersyslog 2> ${RSYSLOG_DYNNAME}.tcpflood

shutdown_when_empty
wait_shutdown

content_check --check-only "parameter tls.tlslib ignored" ${RSYSLOG_DEBUGLOG}
ret=$?
if [ $ret == 0 ]; then
	echo "SKIP: Parameter tls.tlslib not supported"
	skip_test
else
	content_check --check-only "OpenSSL Version too old" ${RSYSLOG_DEBUGLOG}
	ret=$?
	if [ $ret == 0 ]; then
		echo "SKIP: OpenSSL Version too old"
		skip_test
	else
		# Kindly check for a failed session
		content_check "relp connect failed with return 10031" ${RSYSLOG_DYNNAME}.tcpflood
	fi
fi

exit_test