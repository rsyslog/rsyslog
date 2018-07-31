#!/bin/bash
# addd 2018-01-01 by RGerhards, released under ASL 2.0

. $srcdir/diag.sh init
generate_conf
add_conf '
template(name="outfmt" type="string" string="empty-%$!%-\n")
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="13514" ruleset="rs")

ruleset(name="rs") {
	set $. = $!;
	set $! = $.;

	action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")
}
'
startup
. $srcdir/diag.sh tcpflood -m1
shutdown_when_empty
wait_shutdown
EXPECTED='empty--'
echo "$EXPECTED" | cmp - rsyslog.out.log
if [ ! $? -eq 0 ]; then
	echo "FAIL: rsyslog.out.log content invalid:"
	cat $RSYSLOG_OUT_LOG
	echo "Expected:"
	echo "$EXPECTED"
	error_exit 1
fi;
exit_test
