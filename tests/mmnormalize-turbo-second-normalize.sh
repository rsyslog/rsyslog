#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
#
# Copyright 2026 Jérémie Jourdin.
#
# Two turbo mmnormalize actions on one message with the JSON tree
# materialized in between. The second snapshot is merged into the existing
# tree with msgAddJSON semantics: keys from the first normalization survive
# and keys from the second are added (the colliding-key case, where the later
# normalization wins, is covered by the runtime_unit_turbo_msgdup check
# second_attach_merges_into_materialized_tree). Every record's %$!% line must
# carry exactly that key set with matching values.
. ${srcdir:=.}/diag.sh init
require_plugin imtcp
require_plugin mmnormalize

export NUMMESSAGES="${NUMMESSAGES:-1000}"

generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
module(load="../plugins/mmnormalize/.libs/mmnormalize")

input(type="imtcp" port="0"
	listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port"
	ruleset="normalize")

template(name="tree" type="string" string="%$!%\n")

ruleset(name="normalize") {
	action(type="mmnormalize" turbo="on"
		rule="rule=:msgnum:%num:number%:")
	set $.tree = $!;
	action(type="mmnormalize" turbo="on"
		rule="rule=:msgnum:%seq:number%:")
	action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="tree")
}
'
startup
tcpflood -m"$NUMMESSAGES"
shutdown_when_empty
wait_shutdown

lines=$(wc -l < "$RSYSLOG_OUT_LOG")
if [ "$lines" -ne "$NUMMESSAGES" ]; then
	printf 'expected %s tree lines, got %s\n' "$NUMMESSAGES" "$lines"
	error_exit 1
fi
# exactly two keys, same value, num from the first and seq from the second
merged=$(grep -c -E '^\{ *"num": *"?([0-9]+)"?, *"seq": *"?\1"? *\}$' "$RSYSLOG_OUT_LOG")
if [ "$merged" -ne "$NUMMESSAGES" ]; then
	printf 'only %s of %s records carry the merged {num, seq} tree\n' \
		"$merged" "$NUMMESSAGES"
	grep -v -E '^\{ *"num": *"?([0-9]+)"?, *"seq": *"?\1"? *\}$' "$RSYSLOG_OUT_LOG" | head -n 3
	error_exit 1
fi
sed -E 's/^\{ *"num": *"?([0-9]+).*/\1/' "$RSYSLOG_OUT_LOG" > "$RSYSLOG_DYNNAME.num.log"
export SEQ_CHECK_FILE="$RSYSLOG_DYNNAME.num.log"
seq_check 0 $((NUMMESSAGES - 1))
exit_test
