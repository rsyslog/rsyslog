#!/bin/bash
# Test mmjsonparse invalid mode parameter error handling
# This file is part of the rsyslog project, released under ASL 2.0

. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
module(load="../plugins/mmjsonparse/.libs/mmjsonparse")

template(name="outfmt" type="string" string="%msg%\n")

action(type="mmjsonparse" mode="INVALID")
action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")
'

startup
shutdown_when_empty
wait_shutdown
content_check --regex "mmjsonparse: invalid mode 'INVALID'"
exit_test
