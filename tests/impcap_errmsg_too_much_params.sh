#!/bin/bash
# added 2019-12-28 by frikilax
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
module(load="../contrib/impcap/.libs/impcap")

input(type="impcap" interface="lo" file="input.pcap")
action(type="omfile" file="'$RSYSLOG_OUT_LOG'")
'

startup
shutdown_when_empty
wait_shutdown
content_check "impcap: either 'interface' or 'file' must be specified"

exit_test
