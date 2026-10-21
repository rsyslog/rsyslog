#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Exercise omjournal's systemd v256+ namespace-stream path without a live
# journald namespace. The preload fixture records API arguments and returns a
# file-backed stream; after clean shutdown, exact argument and payload checks
# prove the configured namespace, message tag, severity, and body are passed.
# The tag and body intentionally differ so the identifier argument is checked.
. ${srcdir:=.}/diag.sh init
skip_ASAN "LD_PRELOAD conflicts with ASan runtime load order"
require_plugin imtcp
require_plugin omjournal

export RSYSLOG_OMJOURNAL_MOCK_ARGS="$PWD/$RSYSLOG_DYNNAME.namespace.args"
export RSYSLOG_OMJOURNAL_MOCK_STREAM="$PWD/$RSYSLOG_DYNNAME.namespace.stream"
export RSYSLOG_PRELOAD=.libs/liboverride_omjournal_namespace.so

generate_conf
COOKIE=$RSYSLOG_DYNNAME
add_conf '
global(processInternalMessages="off")
module(load="../plugins/imtcp/.libs/imtcp")
module(load="../plugins/omjournal/.libs/omjournal")
input(type="imtcp" address="127.0.0.1" port="0" listenPortFileName="'"$RSYSLOG_DYNNAME"'.tcpflood_port")

action(type="omjournal" namespace="omjournal-test")
'
startup
tcpflood -m1 -M "\"<133>2011-03-01T11:22:12Z host tag msgh RsysLoG-TESTBENCH $COOKIE\""
shutdown_when_empty
wait_shutdown

export EXPECTED="namespace=omjournal-test
identifier=tag
priority=5
level_prefix=1"
cmp_exact "$RSYSLOG_OMJOURNAL_MOCK_ARGS"
content_check "RsysLoG-TESTBENCH $COOKIE" "$RSYSLOG_OMJOURNAL_MOCK_STREAM"
exit_test
