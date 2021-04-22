#!/bin/bash
# added 2015-09-29 by singh.janmejay
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
template(name="outfmt" type="string" string="msg is %$.test%\n")

set $.test = re_extract_i($msg, "msg (.*)", 0, 1, "none");
action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")
'
startup
injectmsg_literal "<167>Jan 16 16:57:54 172.20.245.8 TAG: MsG test1"
injectmsg_literal "<167>Jan 16 16:57:54 172.20.245.8 TAG: MsG test2"
echo doing shutdown
shutdown_when_empty
echo wait on shutdown
wait_shutdown 
content_check "msg is test1"
content_check "msg is test2"
exit_test
