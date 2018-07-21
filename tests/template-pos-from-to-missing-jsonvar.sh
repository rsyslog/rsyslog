#!/bin/bash
# addd 2016-03-28 by RGerhards, released under ASL 2.0

. $srcdir/diag.sh init
generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="13514")

template(name="outfmt" type="string" string="-%$!non!existing!var:109:116:%-\n")
:msg, contains, "msgnum:" action(type="omfile" template="outfmt"
			         file="rsyslog.out.log")
'
startup
. $srcdir/diag.sh tcpflood -m1
shutdown_when_empty
wait_shutdown
echo "--" | cmp - rsyslog.out.log
if [ ! $? -eq 0 ]; then
  echo "invalid output generated, rsyslog.out.log is:"
  cat rsyslog.out.log
  echo "expected was:"
  echo "--"
  exit 1
fi;
exit_test
