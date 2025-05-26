#!/bin/bash
# addd 2018-09-28 by RGerhards, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
module(load="../plugins/omtesting/.libs/omtesting")
global(workDirectory="'${RSYSLOG_DYNNAME}'.spool")
main_queue(queue.filename="mainq"
	queue.type="disk"
	queue.maxDiskSpace="4m"
	queue.maxfilesize="1m"
	queue.timeoutenqueue="300000"
	queue.lowwatermark="5000"

	queue.cry.provider="gcry"
	queue.cry.key="1234567890123456"
	queue.saveonshutdown="on"
)

template(name="outfmt" type="string"
	 string="%msg:F,58:2%,%msg:F,58:3%,%msg:F,58:4%\n")

:omtesting:sleep 0 5000
:msg, contains, "msgnum:" action(type="omfile" template="outfmt"
			         file=`echo $RSYSLOG_OUT_LOG`)
'
startup
injectmsg 0 1000
shutdown_immediate
wait_shutdown
echo INFO: queue files in ${RSYSLOG_DYNNAME}.spool:
ls -l ${RSYSLOG_DYNNAME}.spool
check_not_present "msgnum:0000" ${RSYSLOG_DYNNAME}.spool/mainq.0*
exit_test
