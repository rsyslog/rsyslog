#!/bin/bash
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
export ES_PORT=19200
export NUMMESSAGES=5000 # test is pretty slow, so use a low number
download_elasticsearch
prepare_elasticsearch
start_elasticsearch

init_elasticsearch
generate_conf
add_conf '
template(name="tpl" type="string"
	 string="{\"msgnum\":\"%msg:F,58:2%\"}")

module(load="../plugins/omelasticsearch/.libs/omelasticsearch")

if $msg contains "msgnum:" then {
	action(type="omelasticsearch"
	       server="127.0.0.1"
	       serverport=`echo $ES_PORT`
	       template="tpl"
	       searchIndex="rsyslog_testbench")

	# this action just to count processed messages
	action(type="omfile" file="'$RSYSLOG_DYNNAME'.syncfile")
}
'
startup_vgthread
injectmsg 0 $NUMMESSAGES
wait_file_lines --delay 500 $RSYSLOG_DYNNAME.syncfile $NUMMESSAGES 500
shutdown_when_empty
wait_shutdown_vg 
check_exit_vg
es_getdata $NUMMESSAGES $ES_PORT
seq_check  0 $((NUMMESSAGES - 1))
cleanup_elasticsearch
exit_test
