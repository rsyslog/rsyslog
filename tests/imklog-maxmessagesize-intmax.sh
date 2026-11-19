#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 Rainer Gerhards and Adiscon GmbH.
# Verify that imklog works with the largest accepted maxMessageSize.
# imklog sizes its receive buffer from maxMessageSize plus one byte for
# the terminating NUL. With maxMessageSize=2147483647 that size must be
# computed without signed int overflow; sanitizer builds abort on it.
# LogPath points to a regular file with one line.
# Oracle: the line arrives unchanged and rsyslogd shuts down cleanly.
# Without sanitizers this test also passes on unfixed code; it is a
# regression check for the sanitizer builds.
. ${srcdir:=.}/diag.sh init
require_plugin imklog
printf 'msgnum:1\n' > "$RSYSLOG_DYNNAME.kmsg"
generate_conf
add_conf '
global(maxMessageSize="2147483647")
module(load="../plugins/imklog/.libs/imklog" logpath="'$RSYSLOG_DYNNAME.kmsg'")

template(name="outfmt" type="string" string="%msg%\n")
:msg, contains, "msgnum:" action(type="omfile" template="outfmt" file="'$RSYSLOG_OUT_LOG'")
'
startup
wait_file_lines "$RSYSLOG_OUT_LOG" 1
shutdown_when_empty
wait_shutdown
export EXPECTED="msgnum:1"
cmp_exact
exit_test
