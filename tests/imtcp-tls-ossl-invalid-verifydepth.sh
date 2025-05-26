#!/bin/bash
# add 2019-02-26 by Rainer Gerhards, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
module(	load="../plugins/imtcp/.libs/imtcp"
	StreamDriver.Name="ossl"
	streamdriver.TlsVerifyDepth="1" )

# input is not really needed, just given for completeness
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")
action(type="omfile" file="'$RSYSLOG_OUT_LOG'")
'

startup
shutdown_when_empty
wait_shutdown
content_check "streamdriver.TlsVerifyDepth must be 2 or higher"

exit_test
