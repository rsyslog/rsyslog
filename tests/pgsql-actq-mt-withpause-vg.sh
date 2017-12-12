#!/bin/bash
# This file is part of the rsyslog project, released under ASL 2.0

. $srcdir/diag.sh init

psql -h localhost -U postgres -f testsuites/pgsql-basic.sql

. $srcdir/diag.sh generate-conf
. $srcdir/diag.sh add-conf '
module(load="../plugins/ompgsql/.libs/ompgsql")
if $msg contains "msgnum" then {
	action(type="ompgsql" server="127.0.0.1"
		db="syslogtest" user="postgres" pass="testbench"
		queue.size="10000" queue.type="linkedList"
		queue.workerthreads="5"
		queue.workerthreadMinimumMessages="500"
		queue.timeoutWorkerthreadShutdown="1000"
		queue.timeoutEnqueue="10000"
	)
}'
. $srcdir/diag.sh startup-vg
. $srcdir/diag.sh injectmsg  0 50000
. $srcdir/diag.sh wait-queueempty
echo waiting for worker threads to timeout
./msleep 3000
. $srcdir/diag.sh injectmsg  50000 50000
. $srcdir/diag.sh wait-queueempty
echo waiting for worker threads to timeout
./msleep 2000
. $srcdir/diag.sh injectmsg  100000 50000
. $srcdir/diag.sh shutdown-when-empty
. $srcdir/diag.sh wait-shutdown-vg
. $srcdir/diag.sh check-exit-vg

psql -h localhost -U postgres -d syslogtest -f testsuites/pgsql-select-msg.sql -t -A > rsyslog.out.log
. $srcdir/diag.sh seq-check  0 149999
echo cleaning up test database
psql -h localhost -U postgres -c 'DROP DATABASE IF EXISTS syslogtest;'

. $srcdir/diag.sh exit
