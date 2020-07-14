#!/bin/bash
# added 2020-07-14 by Rainer Gerhards, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
echo This test must be run as root with no other active syslogd
if [ "$EUID" -ne 0 ]; then
    exit 77 # Not root, skip this test
fi
generate_conf
add_conf '
module(load="../plugins/imklog/.libs/imklog" permitnonkernelfacility="on" ruleset="klogrs")

template(name="outfmt" type="string" string="%msg:17:$%:%pri%\n")

ruleset(name="klogrs") {
	:msg, contains, "msgnum" action(type="omfile" template="outfmt" file="'$RSYSLOG_OUT_LOG'")
}
'
startup
echo "<115>Mar 10 01:00:00 172.20.245.8 tag: msgnum:1" > /dev/kmsg
shutdown_when_empty
wait_shutdown
export EXPECTED='Mar 10 01:00:00 172.20.245.8 tag: msgnum:1:115'
cmp_exact

exit_test
