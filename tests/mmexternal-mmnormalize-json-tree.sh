#!/bin/bash
# added 2026-05-22 by Codex, released under ASL 2.0
# Regression coverage for issue #689: mmexternal must be able to merge a
# returned $! tree after mmnormalize has already populated message variables.
# The oracle is the configured omfile output after synchronized shutdown, so
# the test verifies the final rsyslog message state rather than plugin stderr.
. ${srcdir:=.}/diag.sh init

generate_conf
add_conf '
module(load="../plugins/mmnormalize/.libs/mmnormalize")
module(load="../plugins/mmexternal/.libs/mmexternal")

template(name="outfmt" type="string" string="%$!all-json%\n")

action(type="mmnormalize" rule=["rule=:%text:rest%"] useRawMsg="off")
action(type="mmexternal" interface.input="fulljson"
	binary="'$PYTHON' '${srcdir}'/testsuites/mmexternal-SegFault-mm-python.py")
action(type="omfile" template="outfmt" file="'$RSYSLOG_OUT_LOG'")
'

startup
injectmsg literal "<13>Mar 10 01:00:00 host tag:test-message"
shutdown_when_empty
wait_shutdown

content_check '"text": "test-message"'
content_check '"sometag": "somevalue"'

exit_test
