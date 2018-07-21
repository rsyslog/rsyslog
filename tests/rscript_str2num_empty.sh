#!/bin/bash
# add 2017-02-09 by Jan Gerhards, released under ASL 2.0
. $srcdir/diag.sh init
generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="13514")

set $!ip!v1 = 1+"";

template(name="outfmt" type="string" string="%!ip%\n")
local4.* action(type="omfile" file="rsyslog.out.log" template="outfmt")
'
startup
. $srcdir/diag.sh tcpflood -m1 -y
shutdown_when_empty
wait_shutdown
echo '{ "v1": 1 }' | cmp - rsyslog.out.log
if [ ! $? -eq 0 ]; then
  echo "invalid function output detected, rsyslog.out.log is:"
  cat rsyslog.out.log
  error_exit 1
fi;
exit_test
