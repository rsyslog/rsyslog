#!/bin/bash
# add 2017-09-21 by Pascal Withopf, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
global(
	defaultNetstreamDriver="gtls"
	defaultNetstreamDriverKeyFile="tls-certs/nokey.pem"
	defaultNetstreamDriverCertFile="tls-certs/cert.pem"
	defaultNetstreamDriverCaFile="tls-certs/ca.pem"
)
module(load="../plugins/imtcp/.libs/imtcp" StreamDriver.Name="gtls" StreamDriver.Mode="1" StreamDriver.AuthMode="anon")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")

template(name="outfmt" type="string" string="%msg:F,58:2%\n")

action(type="omfile" template="outfmt" file=`echo $RSYSLOG_OUT_LOG`)

'
startup
shutdown_when_empty
wait_shutdown

grep "defaultnetstreamdriverkeyfile.*tls-certs/nokey.pem"  $RSYSLOG_OUT_LOG > /dev/null
if [ $? -ne 0 ]; then
        echo
        echo "FAIL: expected error message from missing input file not found.  $RSYSLOG_OUT_LOG is:"
        cat $RSYSLOG_OUT_LOG
        error_exit 1
fi

exit_test
