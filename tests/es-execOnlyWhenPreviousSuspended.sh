#!/bin/bash
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=100 #10000
ensure_elasticsearch_ready

init_elasticsearch

generate_conf
add_conf '
template(name="tpl" type="string" string="{\"msgnum\":\"%msg:F,58:2%\"}")
template(name="tpl2" type="string" string="%msg:F,58:2%\n")

module(load="../plugins/omelasticsearch/.libs/omelasticsearch")

if $msg contains "msgnum:" then {
	action(type="omelasticsearch"
	       server="127.0.0.1"
	       serverport="19200"
	       template="tpl"
	       searchIndex="rsyslog_testbench"
	       action.resumeInterval="2"
	       action.resumeretrycount="1")

	action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="tpl2"
		action.execOnlyWhenPreviousIsSuspended="on")

	# this action just to count processed messages
	action(type="omfile" file="'$RSYSLOG_DYNNAME'.syncfile")
}
'
startup
injectmsg  0 $NUMMESSAGES
wait_file_lines $RSYSLOG_DYNNAME.syncfile $NUMMESSAGES
stop_elasticsearch
./msleep 1000
injectmsg  $NUMMESSAGES 1
wait_file_lines $RSYSLOG_DYNNAME.syncfile $((NUMMESSAGES + 1))
wait_queueempty
injectmsg  $(( NUMMESSAGES + 1 )) $NUMMESSAGES
wait_file_lines $RSYSLOG_DYNNAME.syncfile $((NUMMESSAGES + 1 + NUMMESSAGES))
start_elasticsearch

shutdown_when_empty
wait_shutdown 

seq_check  $(( NUMMESSAGES + 1 )) $(( NUMMESSAGES * 2 ))
es_getdata $NUMMESSAGES 19200
seq_check  0 $(( NUMMESSAGES - 1 ))
exit_test
