#!/bin/bash
# add 2017-12-29 by Rainer Gerhards, released under ASL 2.0
. $srcdir/diag.sh init
generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
module(load="../plugins/mmexternal/.libs/mmexternal")
input(type="imtcp" port="13514")
set $!x = "a";

template(name="outfmt" type="string" string="%msg%\n")

if $msg contains "msgnum:" then {
	action(type="mmexternal" interface.input="fulljson"
		binary="does/not/exist")
}
action(type="omfile" template="outfmt" file="rsyslog.out.log")
'
startup_vg_noleak
. $srcdir/diag.sh tcpflood -m1 -M "\"<129>Mar 10 01:00:00 172.20.245.8 tag:msgnum:1\""
./msleep 500 # let the fork happen and report back!
shutdown_when_empty
wait_shutdown_vg
. $srcdir/diag.sh check-exit-vg

grep 'failed to execute' rsyslog.out.log > /dev/null
if [ $? -ne 0 ]; then
  echo "invalid response generated, rsyslog.out.log is:"
  cat rsyslog.out.log
  error_exit  1
fi;

exit_test
