#!/bin/bash
# addd 2023-01-13 by RGerhards, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
template(name="outfmt" type="string" string="data:%$!my_struc_data%\n")

set $!my_struc_data = substring($STRUCTURED-DATA, 1, -9999999);
local4.debug action(type="omfile" template="outfmt" file="'$RSYSLOG_OUT_LOG'")
'
startup
injectmsg_literal '<167>1 2003-03-01T01:00:00.000Z hostname1 sender - tag [tcpflood@32473 MSGNUM="0"] data'
shutdown_when_empty
wait_shutdown
export EXPECTED='data:'
cmp_exact
exit_test
