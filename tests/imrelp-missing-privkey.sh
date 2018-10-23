#!/bin/bash
# addd 2018-06-05 by RGerhards, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
module(load="../plugins/imrelp/.libs/imrelp")
input(
    type="imrelp"
    tls="on"
    port="1604"
    tls.myCert="nocert"
)

action(type="omfile" file=`echo $RSYSLOG_OUT_LOG`)
'
touch nocert # it is not important that this is a real cert, it just must exist!
startup
shutdown_when_empty # shut down rsyslogd when done processing messages
wait_shutdown
export EXPECTED="no .* private key file"
. $srcdir/diag.sh grep-check
exit_test
