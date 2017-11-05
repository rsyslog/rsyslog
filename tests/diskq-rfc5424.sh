#!/bin/bash
# detect queue corruption based on invalid property bag ordering.
# Note: this mimics an issue actually seen in practice.
# Triggering condition: "json" property (message variables) are present
# and "structured-data" property is also present. Caused rsyslog to
# thrash the queue file, getting messages stuck in it and loosing all
# after the initial problem occurence.
# add 2017-02-08 by Rainer Gerhards, released under ASL 2.0

uname
if [ `uname` = "SunOS" ] ; then
   echo "This test currently does not work on all flavors of Solaris."
   exit 77
fi

. $srcdir/diag.sh init
. $srcdir/diag.sh generate-conf
. $srcdir/diag.sh add-conf '
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="13514" ruleset="rs")


template(name="outfmt" type="string" string="%msg:F,58:2%\n")

ruleset(name="rs2" queue.type="disk" queue.filename="rs2_q"
	queue.spoolDirectory="test-spool") {
	set $!tmp=$msg;
	action(type="omfile" file="rsyslog.out.log" template="outfmt")
}
ruleset(name="rs") {
	set $!tmp=$msg;
	call rs2
}
'
. $srcdir/diag.sh startup
. $srcdir/diag.sh tcpflood -m1000 -y
. $srcdir/diag.sh shutdown-when-empty
. $srcdir/diag.sh wait-shutdown
. $srcdir/diag.sh seq-check 0 999

. $srcdir/diag.sh exit
