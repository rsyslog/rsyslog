#!/bin/bash
#!/bin/bash
# This file is part of the rsyslog project, released under ASL 2.0
echo ===============================================================================
echo \[mysql-act-mt.sh\]: test for mysql with multithread actionq
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
module(load="../plugins/ommysql/.libs/ommysql")

:msg, contains, "msgnum:" {
	action(type="ommysql" server="127.0.0.1"
	db="Syslog" uid="rsyslog" pwd="testbench"
	queue.size="10000" queue.type="linkedList"
	queue.workerthreads="5"
	queue.workerthreadMinimumMessages="500"
	queue.timeoutWorkerthreadShutdown="100"
	queue.timeoutEnqueue="10000"
	)
} 
'
mysql --user=rsyslog --password=testbench < ${srcdir}/testsuites/mysql-truncate.sql
startup


strtnum=0
for i in {1..50}
do
   echo "running iteration $i, startnum: $strtnum"
   injectmsg  $strtnum 5000
   wait_queueempty 
   echo waiting for worker threads to timeout
   ./msleep 1000
   let "strtnum = strtnum+5000"
done


shutdown_when_empty
wait_shutdown 
# note "-s" is required to suppress the select "field header"
mysql -s --user=rsyslog --password=testbench < ${srcdir}/testsuites/mysql-select-msg.sql > $RSYSLOG_OUT_LOG
seq_check
mysql_cleanup_test
exit_test
