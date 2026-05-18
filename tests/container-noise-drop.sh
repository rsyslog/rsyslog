#!/bin/bash
# Verify the container noise-drop lookup-table pattern.
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init

generate_conf
add_conf '
lookup_table(name="container_noise" file="'$RSYSLOG_DYNNAME'.noise-drop.lkp_tbl" reloadOnHUP="on")

template(name="outfmt" type="string" string="%msg%\n")

set $.container_noise_tag = lookup("container_noise", $rawmsg);
if (strlen($.container_noise_tag) > 0) then {
	stop
}

action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")
'
printf '%s\n' \
	'{ "version": 1, "nomatch": "", "type": "regex", "table": [] }' \
	> "$RSYSLOG_DYNNAME.noise-drop.lkp_tbl"

startup
injectmsg_literal '<165>1 2003-03-01T01:00:00.000Z host app - - - empty-table-pass'
wait_queueempty
printf '%s\n' \
	'{ "version": 1, "nomatch": "", "type": "regex", "table": [' \
	'  { "regex": "drop-default", "tag": "drop" }' \
	'] }' > "$RSYSLOG_DYNNAME.noise-drop.lkp_tbl"
issue_HUP
await_lookup_table_reload
injectmsg_literal '<165>1 2003-03-01T01:00:00.000Z host app - - - keep-default'
injectmsg_literal '<165>1 2003-03-01T01:00:01.000Z host app - - - drop-default'
wait_queueempty
shutdown_when_empty
wait_shutdown
content_check "empty-table-pass"
content_check "keep-default"
check_not_present "drop-default"

generate_conf 2
add_conf '
lookup_table(name="container_noise" file="'$RSYSLOG_DYNNAME'.noise-drop-2.lkp_tbl" reloadOnHUP="on")

template(name="outfmt" type="string" string="%msg%\n")

ruleset(name="rs-main" queue.type="direct") {
	set $.container_noise_tag = lookup("container_noise", $rawmsg);
	if (strlen($.container_noise_tag) > 0) then {
		stop
	}
	action(type="omfile" file=`echo $RSYSLOG2_OUT_LOG` template="outfmt")
}

call rs-main
' 2
printf '%s\n' \
	'{ "version": 1, "nomatch": "", "type": "regex", "table": [' \
	'  { "regex": "drop-named", "tag": "drop" }' \
	'] }' > "$RSYSLOG_DYNNAME.noise-drop-2.lkp_tbl"

startup 2
injectmsg2_literal() {
	printf 'injecting msg payload into instance 2: %s\n' "$1"
	printf '%s\n' "$1" | sed -e 's/^/injectmsg literal /g' | $TESTTOOL_DIR/diagtalker -p$IMDIAG_PORT2 || error_exit $?
}
injectmsg2_literal '<165>1 2003-03-01T01:00:00.000Z host app - - - keep-named'
injectmsg2_literal '<165>1 2003-03-01T01:00:01.000Z host app - - - drop-named'
wait_queueempty 2
shutdown_when_empty 2
wait_shutdown 2
custom_content_check "keep-named" "$RSYSLOG2_OUT_LOG"
check_not_present "drop-named" "$RSYSLOG2_OUT_LOG"

exit_test
