#!/bin/bash
# add 2018-06-27 by Pascal Withopf, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
setvar_RS_HOSTNAME
generate_conf
add_conf '
module(load="../plugins/imudp/.libs/imudp")
input(type="imudp" port="'$TCPFLOOD_PORT'" ruleset="ruleset1")

template(name="outfmt" type="string" string="%PRI%,%syslogfacility-text%,%syslogseverity-text%,%hostname%,%programname%,%syslogtag%,%msg%\n")

ruleset(name="ruleset1") {
	action(type="omfile" file=`echo $RSYSLOG_OUT_LOG`
	       template="outfmt")
}

'
startup
tcpflood -m1 -T "udp" -M "\"<27>xapi: [error|xen3|15|Guest liveness monitor D:bca30ab3f1c1|master_connection] Connection to master died. I will continue to retry indefinitely (suppressing future logging of this message)\""
tcpflood -m1 -T "udp" -M "\"This is a message!\""
shutdown_when_empty
wait_shutdown

export EXPECTED="27,daemon,err,$RS_HOSTNAME,xapi,xapi:, [error|xen3|15|Guest liveness monitor D:bca30ab3f1c1|master_connection] Connection to master died. I will continue to retry indefinitely (suppressing future logging of this message)
13,user,notice,This,is,is, a message!"
cmp_exact $RSYSLOG_OUT_LOG

exit_test
