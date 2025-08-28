#!/bin/bash
# add 2019-01-31 by PascalWithopf, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=1000
do_skip=0
generate_conf
add_conf '
module(load="../plugins/imrelp/.libs/imrelp")
input(type="imrelp" port="'$TCPFLOOD_PORT'" tls="on"
		tls.cacert="'$srcdir'/tls-certs/ca.pem"
		tls.mycert="'$srcdir'/tls-certs/cert.pem"
		tls.myprivkey="'$srcdir'/tls-certs/key.pem"
		tls.authmode="certvalid"
		tls.permittedpeer="rsyslog")

template(name="outfmt" type="string" string="%msg:F,58:2%\n")
:msg, contains, "msgnum:" action(type="omfile" template="outfmt"
			         file=`echo $RSYSLOG_OUT_LOG`)
'
startup
./tcpflood -Trelp-tls -acertvalid -p$TCPFLOOD_PORT -m$NUMMESSAGES -x "$srcdir/tls-certs/ca.pem" -z "$srcdir/tls-certs/key.pem" -Z "$srcdir/tls-certs/cert.pem" -Ersyslog 2> $RSYSLOG_DYNNAME.tcpflood
if [ $? -eq 1 ]; then
	cat $RSYSLOG_DYNNAME.tcpflood
	if ! grep "could net set.*certvalid" < "$RSYSLOG_DYNNAME.tcpflood" ; then
		printf "librelp too old, need to skip this test\n"
		do_skip=1
	fi
fi
	cat -n $RSYSLOG_DYNNAME.tcpflood
shutdown_when_empty
wait_shutdown
if [ $do_skip -eq 1 ]; then
	skip_test
fi
seq_check
exit_test
