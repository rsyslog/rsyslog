#!/bin/bash
# Validate mmutf8fix structured data handling for invalid UTF-8.
# mmutf8fix must sanitize pszStrucData so that mmpstrucdata
# parses clean UTF-8 values into the $! property tree.
# add 2026-04 Charles-Antoine Mathieu, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
module(load="../plugins/mmutf8fix/.libs/mmutf8fix")
module(load="../plugins/mmpstrucdata/.libs/mmpstrucdata")
module(load="../plugins/imtcp/.libs/imtcp")

input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port" ruleset="testing")

template(name="outfmt" type="string"
  string="%$!rfc5424-sd!test@32473!clean% | %$!rfc5424-sd!test@32473!dirty% | %msg%\n")

ruleset(name="testing") {
	action(type="mmutf8fix" replacementChar="?")
	action(type="mmpstrucdata")
	action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")
}
'
startup

# Test 1: Valid UTF-8 in SD should be preserved
tcpflood -m1 -M "\"<134>1 2024-01-01T00:00:00Z host app 1 - [test@32473 clean=\\\"hello\\\" dirty=\\\"world\\\"] valid msg\""

# Test 2: Invalid 0xa0 (Latin-1 NBSP) in SD value — should be replaced with ?
# use a file because raw bytes do not survive shell quoting
printf '<134>1 2024-01-01T00:00:00Z host app 2 - [test@32473 clean="ok" dirty="Brain\xa0Twist"] invalid NBSP\n' \
  > "$RSYSLOG_DYNNAME.tmp"
tcpflood -m1 -I "$RSYSLOG_DYNNAME.tmp"
rm "$RSYSLOG_DYNNAME.tmp"

# Test 3: Invalid 0xed (Latin-1 accented char) in SD value — should be replaced
printf '<134>1 2024-01-01T00:00:00Z host app 3 - [test@32473 clean="ok" dirty="Galer\xeda"] invalid latin1\n' \
  > "$RSYSLOG_DYNNAME.tmp"
tcpflood -m1 -I "$RSYSLOG_DYNNAME.tmp"
rm "$RSYSLOG_DYNNAME.tmp"

# Test 4: Valid multi-byte UTF-8 in SD should be preserved (ñ = 0xc3 0xb1)
tcpflood -m1 -M "\"<134>1 2024-01-01T00:00:00Z host app 4 - [test@32473 clean=\\\"ok\\\" dirty=\\\"España\\\"] valid multibyte\""

# Test 5: No structured data — should not crash
tcpflood -m1 -M "\"<134>1 2024-01-01T00:00:00Z host app 5 - - no SD at all\""

shutdown_when_empty
wait_shutdown

echo 'hello | world | valid msg
ok | Brain?Twist | invalid NBSP
ok | Galer?a | invalid latin1
ok | España | valid multibyte
 |  | no SD at all' > "$RSYSLOG_OUT_LOG.expect"

if ! cmp "$RSYSLOG_OUT_LOG.expect" $RSYSLOG_OUT_LOG; then
  echo "invalid response generated, $RSYSLOG_OUT_LOG diff:"
  LANG=C diff --text -u "$RSYSLOG_OUT_LOG.expect" $RSYSLOG_OUT_LOG
  error_exit 1
fi

exit_test
