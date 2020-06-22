#!/bin/bash
# added 2019-12-28 by frikilax
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init

generate_conf
add_conf '
module(load="../contrib/impcap/.libs/impcap")

input(type="impcap" file="no_file")
action(type="omfile" file="'$RSYSLOG_OUT_LOG'")
'

startup
shutdown_when_empty
wait_shutdown
content_check "no_file: No such file or directory"

exit_test
