#!/bin/bash
# add 2016-11-22 by Pascal Withopf, released under ASL 2.0
. $srcdir/diag.sh init
. $srcdir/diag.sh generate-conf
. $srcdir/diag.sh add-conf '
module(load="../plugins/imudp/.libs/imudp")
input(type="imudp" port="514" address="128.98.1.12")
input(type="imudp" port="514" address="128.98.1.12")

template(name="outfmt" type="string" string="-%msg%-\n")
:syslogtag, contains, "tag" action(type="omfile" template="outfmt"
			         file="rsyslog2.out.log")

action(type="omfile" template="outfmt" file="rsyslog.out.log")
'
. $srcdir/diag.sh startup
. $srcdir/diag.sh shutdown-when-empty
. $srcdir/diag.sh wait-shutdown

grep "imudp: Could not create udp listener, ignoring port 514 bind-address 128.98.1.12." rsyslog.out.log > /dev/null
if [ $? -ne 0 ]; then
        echo
        echo "FAIL: expected error message from missing input file not found. rsyslog.out.log is:"
        cat rsyslog.out.log
        . $srcdir/diag.sh error-exit 1
fi

. $srcdir/diag.sh exit
