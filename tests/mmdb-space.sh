#!/bin/bash 
# added 2014-11-17 by singh.janmejay 
# This file is part of the rsyslog project, released under ASL 2.0 
echo =============================================================================== 
echo \[mmdb.sh\]: test for mmdb
# uncomment for debugging support:
#export RSYSLOG_DEBUG="debug nostdout"
#export RSYSLOG_DEBUGLOG="log"
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
template(name="outfmt" type="string" string="%$!iplocation%\n")

module(load="../plugins/mmdblookup/.libs/mmdblookup")
module(load="../plugins/mmnormalize/.libs/mmnormalize")
module(load="../plugins/imptcp/.libs/imptcp")
input(type="imptcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port" ruleset="testing")

ruleset(name="testing") {
	action(type="mmnormalize" rulebase="'$srcdir'/mmdb.rb")
	action(type="mmdblookup" mmdbfile="'$srcdir'/with_space.mmdb" key="$!ip" fields="city" )
	action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")
}'
startup
tcpflood -m 1 -j "202.106.0.20\ "
shutdown_when_empty
wait_shutdown
content_check '{ "city": "Bei ing" }'
exit_test 
