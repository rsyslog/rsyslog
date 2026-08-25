#!/bin/bash
# Verify that omsendertrack recovers from missing, empty, and syntactically or
# semantically corrupt state files without blocking logging. The oracle is a
# clean daemon shutdown, a valid replacement state file, byte-for-byte
# preserved corrupt input, and no command-file placeholder creation.
# This file is part of the rsyslog project, released under ASL 2.0.
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=1
export QUEUE_EMPTY_CHECK_FUNC=wait_file_lines

write_config() {
	local statefile="$1"
	local ignore="$2"
	generate_conf
	add_conf '
module(load="../plugins/omsendertrack/.libs/omsendertrack")
template(name="hostname" type="string" string="%hostname%")
template(name="outfmt" type="string" string="%msg:F,58:2%\n")
action(type="omsendertrack" senderid="hostname" statefile="'"$statefile"'"
       ignoreInvalidStatefile="'"$ignore"'" cmdfile="'"$statefile"'.commands")
:msg, contains, "msgnum:" action(type="omfile" template="outfmt" file="'"$RSYSLOG_OUT_LOG"'")
'
}

run_recovery_case() {
	local name="$1"
	local contents="$2"
	local statefile="${RSYSLOG_DYNNAME}.${name}.state"
	local original="${statefile}.original"

	if [ "$name" != "missing" ]; then
		printf '%s' "$contents" > "$statefile"
		if [ "$name" != "empty" ]; then
			cp "$statefile" "$original"
		fi
	fi
	rm -f "$RSYSLOG_OUT_LOG"
	write_config "$statefile" on
	startup
	injectmsg_literal '<167>Mar  1 01:00:00 sender1.example.net tag msgnum:00000000:'
	shutdown_when_empty
	wait_shutdown
	content_check '"sender":"sender1.example.net"' "$statefile"
	content_check '"messages":1' "$statefile"
	if [ -e "${statefile}.commands" ]; then
		echo "FAIL: omsendertrack created the unimplemented cmdFile for ${name}"
		error_exit 1
	fi
	if [ "$name" != "missing" ] && [ "$name" != "empty" ]; then
		set -- "${statefile}".corrupt.*
		if [ ! -e "$1" ]; then
			echo "FAIL: corrupt state file was not quarantined for ${name}"
			error_exit 1
		fi
		cmp_exact_file "$original" "$1" || error_exit 1 "corrupt state file changed during quarantine"
	fi
}

run_recovery_case missing ''
run_recovery_case empty ''
run_recovery_case truncated '['
run_recovery_case wrong-root '{}'
run_recovery_case trailing '[] trailing'
run_recovery_case bad-entry '[{"sender":"sender1.example.net","messages":"1","firstseen":1,"lastseen":1}]'
run_recovery_case duplicate '[{"sender":"sender1.example.net","messages":1,"firstseen":1,"lastseen":1},{"sender":"sender1.example.net","messages":2,"firstseen":1,"lastseen":2}]'

strict_state="${RSYSLOG_DYNNAME}.strict.state"
printf '[' > "$strict_state"
cp "$strict_state" "${strict_state}.original"
write_config "$strict_state" off
if ../tools/rsyslogd -N1 \
	-M"../plugins/omsendertrack/.libs:../plugins/imdiag/.libs:../runtime/.libs:../tools/.libs:../tools" \
	-f"${TESTCONF_NM}.conf" >"${RSYSLOG_DYNNAME}.strict.log" 2>&1; then
	echo "FAIL: IgnoreInvalidStatefile=off accepted an invalid state file"
	cat "${RSYSLOG_DYNNAME}.strict.log"
	error_exit 1
fi
cmp_exact_file "${strict_state}.original" "$strict_state" || error_exit 1 "strict mode modified invalid state"
content_check 'IgnoreInvalidStatefile is off' "${RSYSLOG_DYNNAME}.strict.log"

exit_test
