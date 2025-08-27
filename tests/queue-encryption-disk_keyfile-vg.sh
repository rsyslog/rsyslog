#!/bin/bash
# add 2018-09-30 by RGerhards, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
module(load="../plugins/omtesting/.libs/omtesting")
global(workDirectory="'${RSYSLOG_DYNNAME}'.spool")
main_queue(queue.filename="mainq"
	queue.type="disk"
	queue.maxDiskSpace="4m"
	queue.maxfilesize="1m"
	queue.timeoutshutdown="20000"
	queue.timeoutenqueue="300000"
	queue.lowwatermark="5000"

	queue.cry.provider="gcry"
	queue.cry.keyfile="'$RSYSLOG_DYNNAME.keyfile'"
	queue.saveonshutdown="on"
)

template(name="outfmt" type="string"
	 string="%msg:F,58:2%,%msg:F,58:3%,%msg:F,58:4%\n")

:omtesting:sleep 0 5000
:msg, contains, "msgnum:" action(type="omfile" template="outfmt"
			         file=`echo $RSYSLOG_OUT_LOG`)
'
printf "1234567890123456" > $RSYSLOG_DYNNAME.keyfile
startup_vg_noleak # we WILL leak due to the immediate shutdown
injectmsg 0 1000
shutdown_immediate
wait_shutdown_vg
check_exit_vg
echo INFO: queue files in ${RSYSLOG_DYNNAME}.spool:
ls -l ${RSYSLOG_DYNNAME}.spool
check_not_present "msgnum:0000" ${RSYSLOG_DYNNAME}.spool/mainq.0*
exit_test
