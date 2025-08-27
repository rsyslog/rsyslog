#!/bin/bash
# add 2019-04-15 by RGerhards, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
template(name="outfmt" type="plugin" plugin="RSYSLOG_TraditionalFileFormat")
action(type="omfile" template="outfmt" file="'$RSYSLOG_OUT_LOG'")
'
startup
injectmsg 0 1
shutdown_when_empty
wait_shutdown
content_check "Mar  1 01:00:00 172.20.245.8 tag msgnum:00000000:"
exit_test
