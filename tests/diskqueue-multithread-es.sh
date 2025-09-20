#!/bin/bash
# This test stresses the DA queue disk subsystem with multiple threads.
# To do so, the in-memory queues are deliberately sized very small.
# NOTE: depending on circumstances, this test frequently starts the
# DAWorkerPool, which shuffles messages over from the main queue to
# the DA queue. It terminates when we reach low water mark. This can
# happen in our test. So the DA worker pool thread is, depending on
# timing, started and shut down multiple times. This is not a problem
# indication!
# The pstats display is for manual review - it helps to see how many
# messages actually went to the DA queue.
# Copyright (C) 2019-10-28 by Rainer Gerhards
# This file is part of the rsyslog project, released under ASL 2.0
export ES_DOWNLOAD=elasticsearch-6.8.23.tar.gz
. ${srcdir:=.}/diag.sh init
export ES_PORT=19200
export NUMMESSAGES=25000
export QUEUE_EMPTY_CHECK_FUNC=es_shutdown_empty_check
ensure_elasticsearch_ready
generate_conf
add_conf '
global(workDirectory="'$RSYSLOG_DYNNAME'.spool")
template(name="tpl" type="string" string="{\"msgnum\":\"%msg:F,58:2%\"}")

main_queue(queue.size="2000")
module(load="../plugins/impstats/.libs/impstats"
	log.syslog="off" log.file="'$RSYSLOG_DYNNAME'.pstats" interval="1")
module(load="../plugins/imtcp/.libs/imtcp")
module(load="../plugins/omelasticsearch/.libs/omelasticsearch")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")
:msg, contains, "msgnum:" {
			action(type="omelasticsearch" name="act-es"
				template="tpl" server="127.0.0.1"
				serverport="'$ES_PORT'"
				searchIndex="rsyslog_testbench"
				bulkmode="on"
				queue.lowwatermark="250"
				queue.highwatermark="1500"
				queue.type="linkedList" queue.size="2000"
				queue.dequeueBatchSize="64" queue.workerThreads="4"
				queue.fileName="actq" queue.workerThreadMinimumMessages="64")
}
'
startup
tcpflood -m$NUMMESSAGES # use tcpflood to get better async processing than injectmsg!
shutdown_when_empty
wait_shutdown 
echo FOR MANUAL REVIEW: pstats
tail $RSYSLOG_DYNNAME.pstats | grep maxqsize
es_getdata $NUMMESSAGES $ES_PORT
seq_check
exit_test
