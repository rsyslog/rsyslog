#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
#
# Copyright 2026 Rainer Gerhards and Adiscon GmbH.
#
# Verify that repeated direct variable reads in one if/else-if selector use a
# lazy first-read snapshot. The first false condition changes $!snap; the next
# condition must still match the cached original while the selected body sees
# the actual changed message value. A braced else-body nested if must instead
# see the live value. Exact output after synchronized shutdown is the oracle
# for selector ordering, cache lifetime, and post-selection state.
# This file is part of the rsyslog project, released under ASL 2.0.
. ${srcdir:=.}/diag.sh init

generate_conf
add_conf '
template(name="outfmt" type="string" string="%$.selector%|%$!snap%\n")

if $msg contains "msgnum:" then {
	set $!snap = "before";
	if ($!snap == "before" and parse_json("{\"snap\":\"after\"}", "\$!") == 0 and $msg == "never") then {
		set $.selector = "wrong";
	} else if ($!snap == "before" and $msg contains "msgnum:") then {
		set $.selector = "snapshot";
	} else {
		set $.selector = "wrong";
	}
	action(type="omfile" file="'"$RSYSLOG_OUT_LOG"'" template="outfmt")
}

if $msg contains "msgnum:" then {
	set $!snap = "before";
	if ($!snap == "before" and parse_json("{\"snap\":\"after\"}", "\$!") == 0 and $msg == "never") then {
		set $.selector = "wrong";
	} else {
		if ($!snap == "after") then {
			set $.selector = "nested-live";
		} else {
			set $.selector = "nested-stale";
		}
	}
	action(type="omfile" file="'"$RSYSLOG_OUT_LOG"'" template="outfmt")
}
'

startup
injectmsg 0 1
shutdown_when_empty
wait_shutdown

export EXPECTED='snapshot|after
nested-live|after'
cmp_exact
exit_test
