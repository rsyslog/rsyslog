#!/bin/bash
# add 2018-10-10 by Jan Gerhards, released under ASL 2.0

. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
template(name="outfmt" type="string" string="%msg%\n")

module(load="../plugins/mmutf8fix/.libs/mmutf8fix")
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" address="127.0.0.1" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port" ruleset="testing")

ruleset(name="testing") {
	action(type="mmutf8fix" replacementChar="?")
	action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")
}'

startup
tcpflood -m1 -M "\"<129>Mar 10 01:00:00 172.20.245.8 tag: this is a test message
<129>Mar 10 01:00:00 172.20.245.8 tag: valid mixed length UTF-8: foo bar Å™Ãƒê™€ä†‘ğŒ°ğ¨
<129>Mar 10 01:00:00 172.20.245.8 tag: valid 2-byte UTF-8: Å™Ãƒ
<129>Mar 10 01:00:00 172.20.245.8 tag: invalid 2-byte UTF-8 (overlong = 0x00) 0xC0 0x80: À€
<129>Mar 10 01:00:00 172.20.245.8 tag: invalid 2-byte UTF-8 (overlong = 0x2E) 0xC0 0xAE: À®
<129>Mar 10 01:00:00 172.20.245.8 tag: invalid 2-byte UTF-8 (overlong = 0x4E) 0xC1 0x8E: Á
<129>Mar 10 01:00:00 172.20.245.8 tag: invalid 2-byte UTF-8 (overlong, invalid continuation) 0xC0 0xEE: Àî
<129>Mar 10 01:00:00 172.20.245.8 tag: invalid 2-byte UTF-8 (overlong, invalid continuation but valid utf8) 0xC0 0x2E: À.
<129>Mar 10 01:00:00 172.20.245.8 tag: invalid 2-byte UTF-8 (overlong, invalid continuation but valid utf8) 0xC0 0xC2 0xA7: ÀÂ§
<129>Mar 10 01:00:00 172.20.245.8 tag: invalid 2-byte UTF-8 (overlong, not enough bytes) 0xC0: À
<129>Mar 10 01:00:00 172.20.245.8 tag: invalid 2-byte UTF-8 (invalid continuation) 0xC2 0xEE: Âî
<129>Mar 10 01:00:00 172.20.245.8 tag: invalid 2-byte UTF-8 (invalid continuation but valid utf8) 0xC2 0x2E: Â.
<129>Mar 10 01:00:00 172.20.245.8 tag: invalid 2-byte UTF-8 (invalid continuation but valid utf8) 0xC2 0xC2 0xA7: ÂÂ§
<129>Mar 10 01:00:00 172.20.245.8 tag: valid 3-byte UTF-8: ê™€ä†‘
<129>Mar 10 01:00:00 172.20.245.8 tag: invalid 3-byte UTF-8 (overlong = 0x2E) 0xE0 0x80 0xAE: à€®
<129>Mar 10 01:00:00 172.20.245.8 tag: invalid 3-byte UTF-8 (overlong = 0x2E0) 0xE0 0x8B 0xA0: à‹ 
<129>Mar 10 01:00:00 172.20.245.8 tag: invalid 3-byte UTF-8 (invalid 2nd continuation) 0xE0 0xE2 0x81: àâ
<129>Mar 10 01:00:00 172.20.245.8 tag: invalid 3-byte UTF-8 (invalid 2nd continuation but valid utf8) 0xE0 0x41 0xA7: àA§
<129>Mar 10 01:00:00 172.20.245.8 tag: invalid 3-byte UTF-8 (invalid 2nd continuation but valid utf8) 0xE0 0xC2 0xA7: àÂ§
<129>Mar 10 01:00:00 172.20.245.8 tag: invalid 3-byte UTF-8 (invalid 3rd continuation) 0xE1 0x85 0xE1: á…á
<129>Mar 10 01:00:00 172.20.245.8 tag: invalid 3-byte UTF-8 (invalid 3rd continuation but valid utf8) 0xE1 0x85 0x41: á…A
<129>Mar 10 01:00:00 172.20.245.8 tag: invalid 3-byte UTF-8 (invalid 3rd continuation but valid utf8) 0xE1 0x85 0xD5 0xB6: á…Õ¶
<129>Mar 10 01:00:00 172.20.245.8 tag: valid 4-byte UTF-8: ğŒ°ğ¨
<129>Mar 10 01:00:00 172.20.245.8 tag: invalid 4-byte UTF-8 (overlong = 0x2E) 0xF0 0x80 0x80 0xAE: ğ€€®
<129>Mar 10 01:00:00 172.20.245.8 tag: invalid 4-byte UTF-8 (overlong = 0x2E0) 0xF0 0x80 0x8B 0xA0: ğ€‹ 
<129>Mar 10 01:00:00 172.20.245.8 tag: invalid 4-byte UTF-8 (overlong = 0x2E00) 0xF0 0x82 0xB8 0x80: ğ‚¸€
<129>Mar 10 01:00:00 172.20.245.8 tag: invalid 4-byte UTF-8 (invalid 2nd continuation but valid utf8) 0xF0 0x41 0xC5 0xB0: ğAÅ°
<129>Mar 10 01:00:00 172.20.245.8 tag: invalid 4-byte UTF-8 (invalid 2nd continuation but valid utf8) 0xF0 0xEC 0x8C 0xB0: ğìŒ°
<129>Mar 10 01:00:00 172.20.245.8 tag: invalid 4-byte UTF-8 (invalid 3rd continuation but valid utf8) 0xF0 0x90 0xC5 0xB0: ğÅ°
<129>Mar 10 01:00:00 172.20.245.8 tag: invalid 4-byte UTF-8 (invalid 4rd continuation but valid utf8) 0xF0 0x90 0xC5 0x2E: ğŒ.
<129>Mar 10 01:00:00 172.20.245.8 tag: special characters: ??%%,,..
<129>Mar 10 01:00:00 172.20.245.8 tag: numbers: 1234567890\""

shutdown_when_empty
wait_shutdown
echo ' this is a test message
 valid mixed length UTF-8: foo bar Å™Ãƒê™€ä†‘ğŒ°ğ¨
 valid 2-byte UTF-8: Å™Ãƒ
 invalid 2-byte UTF-8 (overlong = 0x00) 0xC0 0x80: ??
 invalid 2-byte UTF-8 (overlong = 0x2E) 0xC0 0xAE: ??
 invalid 2-byte UTF-8 (overlong = 0x4E) 0xC1 0x8E: ??
 invalid 2-byte UTF-8 (overlong, invalid continuation) 0xC0 0xEE: ??
 invalid 2-byte UTF-8 (overlong, invalid continuation but valid utf8) 0xC0 0x2E: ?.
 invalid 2-byte UTF-8 (overlong, invalid continuation but valid utf8) 0xC0 0xC2 0xA7: ?Â§
 invalid 2-byte UTF-8 (overlong, not enough bytes) 0xC0: ?
 invalid 2-byte UTF-8 (invalid continuation) 0xC2 0xEE: ??
 invalid 2-byte UTF-8 (invalid continuation but valid utf8) 0xC2 0x2E: ?.
 invalid 2-byte UTF-8 (invalid continuation but valid utf8) 0xC2 0xC2 0xA7: ?Â§
 valid 3-byte UTF-8: ê™€ä†‘
 invalid 3-byte UTF-8 (overlong = 0x2E) 0xE0 0x80 0xAE: ???
 invalid 3-byte UTF-8 (overlong = 0x2E0) 0xE0 0x8B 0xA0: ???
 invalid 3-byte UTF-8 (invalid 2nd continuation) 0xE0 0xE2 0x81: ???
 invalid 3-byte UTF-8 (invalid 2nd continuation but valid utf8) 0xE0 0x41 0xA7: ?A?
 invalid 3-byte UTF-8 (invalid 2nd continuation but valid utf8) 0xE0 0xC2 0xA7: ?Â§
 invalid 3-byte UTF-8 (invalid 3rd continuation) 0xE1 0x85 0xE1: ???
 invalid 3-byte UTF-8 (invalid 3rd continuation but valid utf8) 0xE1 0x85 0x41: ??A
 invalid 3-byte UTF-8 (invalid 3rd continuation but valid utf8) 0xE1 0x85 0xD5 0xB6: ??Õ¶
 valid 4-byte UTF-8: ğŒ°ğ¨
 invalid 4-byte UTF-8 (overlong = 0x2E) 0xF0 0x80 0x80 0xAE: ????
 invalid 4-byte UTF-8 (overlong = 0x2E0) 0xF0 0x80 0x8B 0xA0: ????
 invalid 4-byte UTF-8 (overlong = 0x2E00) 0xF0 0x82 0xB8 0x80: ????
 invalid 4-byte UTF-8 (invalid 2nd continuation but valid utf8) 0xF0 0x41 0xC5 0xB0: ?AÅ°
 invalid 4-byte UTF-8 (invalid 2nd continuation but valid utf8) 0xF0 0xEC 0x8C 0xB0: ?ìŒ°
 invalid 4-byte UTF-8 (invalid 3rd continuation but valid utf8) 0xF0 0x90 0xC5 0xB0: ??Å°
 invalid 4-byte UTF-8 (invalid 4rd continuation but valid utf8) 0xF0 0x90 0xC5 0x2E: ???.
 special characters: ??%%,,..
 numbers: 1234567890' > "$RSYSLOG_OUT_LOG.expect"

if ! cmp "$RSYSLOG_OUT_LOG.expect" $RSYSLOG_OUT_LOG; then
  echo "invalid response generated, $RSYSLOG_OUT_LOG diff:"
  # Use LANG=C for binary matching
  LANG=C diff --text -u "$RSYSLOG_OUT_LOG.expect" $RSYSLOG_OUT_LOG
  error_exit  1
fi;

exit_test

exit 0
