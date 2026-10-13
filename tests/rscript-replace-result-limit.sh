#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 Rainer Gerhards and Adiscon GmbH.
# An accepted input can request a replacement result above maxMessageSize.
# Empty oversized results, an exact normal result, and one captured internal
# warning prove the limit without creating a huge string. A concatenated
# source and search string both exceed the limit: the old in-progress check
# rejected this complete match, while its final one-byte result must pass.
# Clean shutdown rules out a crash.
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
global(maxMessageSize="256")
template(name="outfmt" type="string"
         string="normal=%$.normal%;replace=\"%$.large%\";wrap=\"%$.wrapped%\";wrap2=\"%$.wrapped2%\";shrink=\"%$.shrink%\";source_len=%$.source_len%\n")

module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" address="127.0.0.1" port="0" ruleset="replace_limit"
      listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")

ruleset(name="replace_limit") {
    set $.normal = replace("aba", "a", "c");
    set $.large = replace($msg, "a", "bb");
    set $.wrapped = wrap($msg, "a", "bb");
    set $.wrapped2 = wrap($msg, $msg);
    set $.shrink = replace($msg & $msg, $msg & $msg, "b");
    set $.source_len = strlen($msg & $msg);
    action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")
}
'
printf '<13>Oct 10 00:00:00 host tag: ' > "$RSYSLOG_DYNNAME.input"
printf '%150s' '' | tr ' ' a >> "$RSYSLOG_DYNNAME.input"
printf '\n' >> "$RSYSLOG_DYNNAME.input"

startup
tcpflood -m1 -I "$RSYSLOG_DYNNAME.input"
shutdown_when_empty
wait_shutdown
content_check 'normal=cbc;replace="";wrap="";wrap2=""'
content_check 'shrink="b"'
content_check 'source_len=302'
content_count_check 'RainerScript replace()/wrap() result exceeds maxMessageSize' 1 "$RSYSLOG_DYNNAME.started"
exit_test
