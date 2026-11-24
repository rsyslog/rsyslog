#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# This is part of the rsyslog testbench, licensed under ASL 2.0
# Verify option.json escapes quote-only, backslash-only, and mixed field values.
# The exact output bytes must match independent JSON encoding after shutdown.
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
set $!backslash = "a \\ \"b\" c / d";
set $!all_mixed = "\"\\\"";
set $!all_quotes = "\"\"\"";
set $!all_slashes = "\\\\\\";

template(name="json" type="list" option.json="on") {
        constant(value="{\"backslash\":\"")
        property(name="$!backslash")
        constant(value="\",\"quotes\":\"")
        property(name="$!all_quotes")
        constant(value="\",\"slashes\":\"")
        property(name="$!all_slashes")
        constant(value="\",\"mixed\":\"")
        property(name="$!all_mixed")
        constant(value="\"}\n")
}

:msg, contains, "msgnum:" action(type="omfile" template="json"
                                 file=`echo $RSYSLOG_OUT_LOG`)
'
startup
injectmsg 0 1
shutdown_when_empty
wait_shutdown

export EXPECTED='{"backslash":"a \\ \"b\" c / d","quotes":"\"\"\"","slashes":"\\\\\\","mixed":"\"\\\""}'
cmp_exact

exit_test
