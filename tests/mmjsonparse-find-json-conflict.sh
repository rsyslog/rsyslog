#!/bin/bash
# Regression test for mmjsonparse find-json ownership on msgAddJSON failure.
# The action targets a nested JSON path whose parent is already a scalar, so
# msgAddJSON rejects the insertion after consuming the parsed object. Success is
# rsyslog staying alive and continuing to the following omfile action; under the
# Valgrind wrapper this also proves the object is not released a second time.
# This file is part of the rsyslog project, released under ASL 2.0

. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
module(load="../plugins/mmjsonparse/.libs/mmjsonparse")

template(name="outfmt" type="string" string="%msg% parsesuccess=%parsesuccess% json=%$!%\n")

if $msg contains "CONFLICT" then {
    set $!conflict = "scalar";
    action(type="mmjsonparse" mode="find-json" container="$!conflict!parsed")
    action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
}
'
startup
injectmsg_literal '<167>Jan 16 16:57:54 host.example.net TAG: CONFLICT prefix {"field":"value"}'
shutdown_when_empty
wait_shutdown

export EXPECTED=' CONFLICT prefix {"field":"value"} parsesuccess=FAIL json={ "conflict": "scalar" }'
cmp_exact
exit_test
