#!/bin/bash
# addd 2019-01-31 by PascalWithopf, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=1000
do_skip=0
generate_conf
add_conf '
module(load="../plugins/imrelp/.libs/imrelp")
input(type="imrelp" port="'$TCPFLOOD_PORT'" tls="on"
		tls.cacert="'$srcdir'/tls-certs/ca.pem"
		tls.mycert="'$srcdir'/tls-certs/dont-exist.pem"
		tls.myprivkey="'$srcdir'/tls-certs/key.pem"
		tls.authmode="anon"
		tls.permittedpeer="rsyslog")

template(name="outfmt" type="string" string="%msg:F,58:2%\n")
:msg, contains, "msgnum:" action(type="omfile" template="outfmt"
			         file=`echo $RSYSLOG_OUT_LOG`)
'
startup
tcpflood -Trelp-plain -p$TCPFLOOD_PORT -m$NUMMESSAGES
shutdown_when_empty
wait_shutdown
if [ $do_skip -eq 1 ]; then
	skip_test
fi
seq_check
exit_test
