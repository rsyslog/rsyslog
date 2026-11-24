#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
#
# Copyright 2026 Jérémie Jourdin.
#
# A full-tree "set $! = ..." onto an existing $! merges only when both are
# objects. An object assigned over a non-object root replaces it, and a
# non-object assigned over an object root is rejected, as for a leaf. Both
# used to call json_object_object_add() or iterate a non-object and crash.
. ${srcdir:=.}/diag.sh init
require_plugin imdiag
generate_conf
add_conf '
template(name="outfmt" type="string" string="%$!all-json%\n")
if $msg contains "msgnum:" then {
	set $.obj!k = "v";
	set $! = "text";
	set $! = $.obj;
	action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
	set $!a = "1";
	set $! = "scalar";
	action(type="omfile" file="'$RSYSLOG2_OUT_LOG'" template="outfmt")
}
'
startup
injectmsg 0 1
shutdown_when_empty
wait_shutdown

export EXPECTED='{ "k": "v" }'
cmp_exact "$RSYSLOG_OUT_LOG"
export EXPECTED='{ "k": "v", "a": "1" }'
cmp_exact "$RSYSLOG2_OUT_LOG"
exit_test
