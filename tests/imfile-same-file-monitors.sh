#!/bin/bash
# Verifies that multiple imfile monitors configured for the same file receive
# each completed line independently. The oracle is the final omfile output
# after synchronized shutdown: the two unrestricted monitors must both receive
# a startup canary and both test lines. A third rate-limited monitor consumes
# its quota on the canary and must then drop only its own later copies without
# suppressing the unrestricted monitors.
# This is part of the rsyslog testbench, licensed under ASL 2.0
. ${srcdir:=.}/diag.sh init
. $srcdir/diag.sh check-inotify

mkdir "$RSYSLOG_DYNNAME.work"
generate_conf
add_conf '
global(workDirectory="./'$RSYSLOG_DYNNAME'.work")
module(load="../plugins/imfile/.libs/imfile" mode="inotify")

input(type="imfile" File="./'$RSYSLOG_DYNNAME'.input" Tag="first:")
input(type="imfile" File="./'$RSYSLOG_DYNNAME'.input" Tag="second:")
input(type="imfile" File="./'$RSYSLOG_DYNNAME'.input" Tag="limited:" maxLinesPerMinute="1")

template(name="outfmt" type="list") {
	property(name="syslogtag")
	constant(value=" ")
	property(name="msg")
	constant(value="\n")
}

if $msg contains "same-file-monitor-" then {
	action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
}
'

touch "$RSYSLOG_DYNNAME.input"
startup

printf 'same-file-monitor-canary\n' >> "$RSYSLOG_DYNNAME.input"
content_check_with_count "same-file-monitor-canary" 3 60

printf 'same-file-monitor-1\nsame-file-monitor-2\n' >> "$RSYSLOG_DYNNAME.input"

content_check_with_count "same-file-monitor-" 7 60
shutdown_when_empty
wait_shutdown

presort
# EXPECTED is consumed by cmp_exact.
# shellcheck disable=SC2034
EXPECTED='first: same-file-monitor-1
first: same-file-monitor-2
first: same-file-monitor-canary
limited: same-file-monitor-canary
second: same-file-monitor-1
second: same-file-monitor-2
second: same-file-monitor-canary'
cmp_exact "$RSYSLOG_DYNNAME.presort"

exit_test
