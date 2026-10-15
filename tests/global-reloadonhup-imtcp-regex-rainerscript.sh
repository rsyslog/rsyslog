#!/bin/bash
# Verify that an imtcp delimiter-regex change is a transactional new-session
# update. The pre-HUP socket must keep splitting on the old regex, while a
# post-HUP socket must split only on the new regex. Each visible first frame is
# released by a second same-generation marker, so completion is content-based
# and does not depend on connection close or elapsed time.
. ${srcdir:=.}/diag.sh init
require_plugin imtcp
generate_conf
add_conf '
global(config.reloadOnHUP="on")
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" address="127.0.0.1" port="0"
      listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port"
      name="regex" ruleset="main" framing.delimiter.regex="^<33>Mar")
ruleset(name="main") {
    action(type="omfile" name="regex_sink" file="'$RSYSLOG_OUT_LOG'")
}
'
startup
exec 9<>"/dev/tcp/127.0.0.1/$TCPFLOOD_PORT"
printf '%s\n' \
	'<33>Mar  1 01:00:00 host app: regex-old-before' \
	'<33>Mar  1 01:00:01 host app: regex-old-before-flush' >&9 || error_exit 1
wait_content 'regex-old-before' "$RSYSLOG_OUT_LOG"
cp "$CONF_FILE" "$CONF_FILE.base"

sed 's/\^<33>Mar/\^<34>Apr/' "$CONF_FILE.base" >"$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=activated active_generation=2"* ||
      "$reload_status" != *"modified=1 invalid=0 source_capability=new_sessions"* ]]; then
	echo "FAIL: RainerScript delimiter regex did not activate for new sessions: $reload_status"
	error_exit 1
fi

# The established session owns its compiled old regex and remains usable.
printf '%s\n' \
	'<33>Mar  1 01:00:02 host app: regex-old-after' \
	'<33>Mar  1 01:00:03 host app: regex-old-after-flush' >&9 || error_exit 1
wait_content 'regex-old-after' "$RSYSLOG_OUT_LOG"

# A later accept compiles the newly published regex. The old pattern would not
# release either of these frames, so visible output proves the new profile won.
exec 8<>"/dev/tcp/127.0.0.1/$TCPFLOOD_PORT"
printf '%s\n' \
	'<34>Apr  1 01:00:00 host app: regex-new-after' \
	'<34>Apr  1 01:00:01 host app: regex-new-after-flush' >&8 || error_exit 1
wait_content 'regex-new-after' "$RSYSLOG_OUT_LOG"

# Regex compilation belongs to Prepare. An invalid candidate must fail before
# publication and leave both established session-owned regexes untouched.
cp "$CONF_FILE" "$CONF_FILE.active"
sed 's/\^<34>Apr/[/' "$CONF_FILE.active" >"$CONF_FILE.candidate"
mv "$CONF_FILE.candidate" "$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=activation_failed active_generation=2"* ||
      "$reload_status" != *"modified=1 invalid=0 source_capability=new_sessions"* ]]; then
	echo "FAIL: invalid RainerScript regex did not roll back Prepare: $reload_status"
	error_exit 1
fi
printf '%s\n' \
	'<33>Mar  1 01:00:04 host app: regex-old-after-rollback' \
	'<33>Mar  1 01:00:05 host app: regex-old-after-rollback-flush' >&9 || error_exit 1
printf '%s\n' \
	'<34>Apr  1 01:00:02 host app: regex-new-after-rollback' \
	'<34>Apr  1 01:00:03 host app: regex-new-after-rollback-flush' >&8 || error_exit 1
wait_content 'regex-old-after-rollback' "$RSYSLOG_OUT_LOG"
wait_content 'regex-new-after-rollback' "$RSYSLOG_OUT_LOG"

exec 8>&-
exec 9>&-
shutdown_when_empty
wait_shutdown
exit_test
