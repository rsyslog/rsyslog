#!/bin/bash
# Test re_extract behavior with empty matches to prevent infinite loops/stuck behavior
echo ===============================================================================
echo \[rscript_re_extract_loop.sh\]: test re_extract empty match loop
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
template(name="outfmt" type="string" string="*Result: %$.res%*\n")

module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")'

# Regex: a?
# Input: " a" (space then a)
# Match 0: "" (at start, before space)
# Match 1: "a" (after space)
# We want Match 1.
add_conf "
set \$.res = re_extract(\$msg, 'a?', 1, 0, 'fail');"

add_conf '
action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")
'
export RSYSLOG_DEBUG="debug nostdout"
export RSYSLOG_DEBUGLOG="$RSYSLOG_DYNNAME.debug"

startup
# Send proper syslog message so $msg is populated
# Double quoting for eval in diag.sh
tcpflood -m 1 -M '"<166>Jan 1 00:00:00 localhost tag: a"'
echo doing shutdown
shutdown_when_empty
echo wait on shutdown
wait_shutdown
content_check "*Result: a*"
exit_test
