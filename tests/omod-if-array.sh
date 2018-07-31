#!/bin/bash
# add 2018-06-29 by Pascal Withopf, released under ASL 2.0
. $srcdir/diag.sh init
generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="13514" ruleset="ruleset1")

template(name="outfmt" type="string" string="%PRI%%timestamp%%hostname%%programname%%syslogtag%\n")

ruleset(name="ruleset1") {
	action(type="omfile" file=`echo $RSYSLOG_OUT_LOG`
	       template="outfmt")
}

'
startup
. $srcdir/diag.sh tcpflood -m1 -M "\"<167>Mar  6 16:57:54 172.20.245.8 %PIX-7-710005: UDP request discarded from SERVER1/2741 to test_app:255.255.255.255/61601\""
shutdown_when_empty
wait_shutdown

echo '167Mar  6 16:57:54172.20.245.8%PIX-7-710005%PIX-7-710005:' | cmp - rsyslog.out.log
if [ ! $? -eq 0 ]; then
  echo "invalid response generated, rsyslog.out.log is:"
  cat $RSYSLOG_OUT_LOG
  error_exit  1
fi;

exit_test
