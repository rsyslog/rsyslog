#!/bin/bash
# add 2018-06-27 by Pascal Withopf, released under ASL 2.0
. $srcdir/diag.sh init
setvar_RS_HOSTNAME
. $srcdir/diag.sh generate-conf
. $srcdir/diag.sh add-conf '
module(load="../plugins/imudp/.libs/imudp")
input(type="imudp" port="13514" ruleset="ruleset1")

template(name="outfmt" type="string" string="%PRI%,%syslogfacility-text%,%syslogseverity-text%,%hostname%,%programname%,%syslogtag%,%msg%\n")

ruleset(name="ruleset1") {
	action(type="omfile" file="rsyslog.out.log"
	       template="outfmt")
}

'
. $srcdir/diag.sh startup
. $srcdir/diag.sh tcpflood -m1 -T "udp" -M "\"<27>xapi: [error|xen3|15|Guest liveness monitor D:bca30ab3f1c1|master_connection] Connection to master died. I will continue to retry indefinitely (supressing future logging of this message)\""
. $srcdir/diag.sh tcpflood -m1 -T "udp" -M "\"This is a message!\""
. $srcdir/diag.sh shutdown-when-empty
. $srcdir/diag.sh wait-shutdown

export EXPECTED="27,daemon,err,$RS_HOSTNAME,xapi,xapi:, [error|xen3|15|Guest liveness monitor D:bca30ab3f1c1|master_connection] Connection to master died. I will continue to retry indefinitely (supressing future logging of this message)
13,user,notice,This,is,is, a message!"
cmp_exact rsyslog.out.log

. $srcdir/diag.sh exit
