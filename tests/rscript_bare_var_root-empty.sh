#!/bin/bash
# addd 2018-01-01 by RGerhards, released under ASL 2.0

. $srcdir/diag.sh init
. $srcdir/diag.sh generate-conf
. $srcdir/diag.sh add-conf '
template(name="outfmt" type="string" string="empty-%$!%-\n")
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="13514" ruleset="rs")

ruleset(name="rs") {
	set $. = $!;
	set $! = $.;

	action(type="omfile" file="rsyslog.out.log" template="outfmt")
}
'
. $srcdir/diag.sh startup
. $srcdir/diag.sh tcpflood -m1
. $srcdir/diag.sh shutdown-when-empty
. $srcdir/diag.sh wait-shutdown
EXPECTED='empty--'
echo "$EXPECTED" | cmp - rsyslog.out.log
if [ ! $? -eq 0 ]; then
	echo "FAIL: rsyslog.out.log content invalid:"
	cat rsyslog.out.log
	echo "Expected:"
	echo "$EXPECTED"
	. $srcdir/diag.sh error-exit 1
fi;
. $srcdir/diag.sh exit
