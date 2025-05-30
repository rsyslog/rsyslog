#!/bin/bash
# addd 2019-12-27 by RGerhards, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")
global(parser.PermitSlashInProgramname="off")

template(name="outfmt" type="string" string="%syslogtag%,%programname%,%app-name%\n")
local0.* action(type="omfile" template="outfmt" file="'$RSYSLOG_OUT_LOG'")
'
startup
tcpflood -m 1 -M "\"<133>Aug  6 16:57:54 host /no-app-name msgh ...x\""
shutdown_when_empty
wait_shutdown
export EXPECTED="/no-app-name,,-"
cmp_exact
exit_test
