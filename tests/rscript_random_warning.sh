#!/bin/bash
## rscript_random_warning.sh
## Verify that random() warns when max exceeds platform limit
. ${srcdir:=.}/diag.sh init
export RS_REDIR=">rsyslog.log 2>&1"

generate_conf
add_conf '
$DebugFile rsyslog.debug.log
$DebugLevel 2

template(name="outfmt" type="string" string="%$.rand%\n")
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="0" listenPortFileName="'${RSYSLOG_DYNNAME}'.tcpflood_port")
set $.rand = random(4294967296);
action(type="omfile" file="'${RSYSLOG_OUT_LOG}'" template="outfmt")
'
startup
tcpflood -m 1
shutdown_when_empty
wait_shutdown
WARN="rainerscript: desired random-number range [0 - 4294967296] is wider than supported limit"
if ! grep -F "$WARN" rsyslog.log > /dev/null; then
  echo "FAIL: warning not found in rsyslog.log"
  cat rsyslog.log
  exit 1
fi
if ! grep -F "$WARN" rsyslog.debug.log > /dev/null; then
  echo "FAIL: warning not found in rsyslog.debug.log"
  cat rsyslog.debug.log
  exit 1
fi
exit_test
