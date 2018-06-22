#!/bin/bash
# This file is part of the rsyslog project, released under ASL 2.0
export ES_DOWNLOAD=elasticsearch-6.0.0.tar.gz
. $srcdir/diag.sh download-elasticsearch
. $srcdir/diag.sh stop-elasticsearch
. $srcdir/diag.sh prepare-elasticsearch
. $srcdir/diag.sh start-elasticsearch

. $srcdir/diag.sh init
. $srcdir/diag.sh es-init
. $srcdir/diag.sh generate-conf
. $srcdir/diag.sh add-conf '
template(name="tpl" type="string"
	 string="{\"msgnum\":\"%msg:F,58:2%\"}")


ruleset(name="stats") {
  action(type="omfile" file="./rsyslog.out.stats.log")
}
module(load="../plugins/impstats/.libs/impstats" interval="1" severity="7" resetCounters="off" Ruleset="stats" bracketing="on" format="json")

module(load="../plugins/omelasticsearch/.libs/omelasticsearch")
:msg, contains, "msgnum:" action(type="omelasticsearch"
				 server=["localhost", "http://localhost/", "localhost:9201"]
				 serverport="19200"
				 template="tpl"
				 searchIndex="rsyslog_testbench")
'
. $srcdir/diag.sh startup
. $srcdir/diag.sh injectmsg  0 100
. $srcdir/diag.sh wait-queueempty
. $srcdir/diag.sh wait-for-stats-flush 'rsyslog.out.stats.log'
. $srcdir/diag.sh shutdown-when-empty
. $srcdir/diag.sh wait-shutdown 
. $srcdir/diag.sh es-getdata 100 19200
. $srcdir/diag.sh seq-check  0 99
# The configuration makes every other request from message #3 fail checkConn (N/2-1)
. $srcdir/diag.sh custom-content-check '"failed.checkConn": 49' 'rsyslog.out.stats.log'
. $srcdir/diag.sh cleanup-elasticsearch
. $srcdir/diag.sh exit
