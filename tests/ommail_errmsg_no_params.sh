#!/bin/bash
# add 2018-09-25 by Jan Gerhards, released under ASL 2.0

. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
template(name="outfmt" type="string" string="%msg%\n")

module(load="../plugins/ommail/.libs/ommail")

action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")
action(type="ommail")
'

startup
shutdown_when_empty
wait_shutdown
content_check "parameter 'mailto' required but not specified"

exit_test
