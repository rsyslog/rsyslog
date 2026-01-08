#!/bin/bash
# add 2021-01 Jan Jeronym Zvanovec <jero@zvano.net>, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '

module(load="../plugins/mmutf8fix/.libs/mmutf8fix")
module(load="../plugins/imtcp/.libs/imtcp")

input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port" ruleset="customparser")

parser(name="custom.rfc3164" type="pmrfc3164" force.tagEndingByColon="on")
template(name="outfmt" type="string" string="-%syslogtag%-%msg%-\n")

ruleset(name="customparser" parser="custom.rfc3164") {
	action(type="mmutf8fix" replacementChar="?")
	action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")
}
'
startup
tcpflood -m1 -M "\"<129>Mar 10 01:00:00 Hostname1 Å:\""
tcpflood -m1 -M "\"<129>mar 10 01:00:00 hostname1 À:\""
broken_utf=$(echo a0aa44e45ec47b2893680a |xxd -r -p) 
tcpflood -m1 -M "\"<129>mar 10 01:00:00 hostname1 ${broken_utf}:\""
shutdown_when_empty
wait_shutdown

echo -e '-Å:--\n-À:--\n-??D?^?{(?h:--' > "$RSYSLOG_OUT_LOG.expect"

if ! cmp "$RSYSLOG_OUT_LOG.expect" $RSYSLOG_OUT_LOG; then
  echo "invalid response generated, $RSYSLOG_OUT_LOG diff:"
  # Use LANG=C for binary matching
  LANG=C diff --text -u "$RSYSLOG_OUT_LOG.expect" $RSYSLOG_OUT_LOG
  error_exit  1
fi;

exit_test
