#!/bin/bash
# addd 2016-07-11 by RGerhards, released under ASL 2.0

. $srcdir/diag.sh init
generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="13514")
template(name="outfmt" type="string" string="%hostname%\n")

$rulesetparser rsyslog.rfc5424
local4.debug action(type="omfile" template="outfmt" file=`echo $RSYSLOG_OUT_LOG`)
'
startup
echo '<167>1 2003-03-01T01:00:00.000Z hostname1/hostname2 tcpflood - tag [tcpflood@32473 MSGNUM="0"] data' > rsyslog.input
. $srcdir/diag.sh tcpflood -B -I rsyslog.input
shutdown_when_empty
wait_shutdown
echo "hostname1/hostname2" | cmp - rsyslog.out.log
if [ ! $? -eq 0 ]; then
  echo "invalid hostname generated, rsyslog.out.log is:"
  cat rsyslog.out.log
  error_exit 1
fi;
exit_test
