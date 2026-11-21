#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Check legacy-use and actual TCP-framing warnings through the configured output.
# Repeated safe/unsafe records must produce one warning of each applicable kind
# per process. Exact payloads, output-count readiness, and synchronized shutdown
# prove delivery and warning bounds; plain, disabled, and stream controls are quiet.
. "${srcdir:=.}/diag.sh" init
module="${LEGACY_WARNING_INPUT:-imtcp}"
require_plugin "$module"
check_command_available python3

for mode in safe mixed plain disabled stream; do
	generate_conf
	export NUMMESSAGES=20
	[ "$mode" != mixed ] || export NUMMESSAGES=40
	RSYSLOG_OUT_LOG="$RSYSLOG_DYNNAME.$mode.records"
	diagnostics="$RSYSLOG_DYNNAME.$mode.diagnostics"
	portfile="$RSYSLOG_DYNNAME.$mode.port"
	: > "$diagnostics"
	: > "$RSYSLOG_OUT_LOG"
	options=
	[ "$mode" != disabled ] || add_conf 'global(parser.supportCompressionExtension="off")'
	[ "$mode" != stream ] || options='compression.mode="stream:always"'
	add_conf '
global(processInternalMessages="on")
module(load="../plugins/'"$module"'/.libs/'"$module"'")
input(type="'"$module"'" address="127.0.0.1" port="0"
      listenPortFileName="'"$portfile"'" ruleset="received" '"$options"')
template(name="rawline" type="string" string="%rawmsg%\n")
if $syslogtag contains "rsyslogd" then {
	action(type="omfile" file="'"$diagnostics"'" template="rawline")
	stop
}
ruleset(name="received") {
	action(type="omfile" file="'"$RSYSLOG_OUT_LOG"'" template="rawline")
}
'
	startup
	wait_file_exists "$portfile"
	python3 "$srcdir/testsuites/legacy-zlib-warnings.py" "$mode" "$(cat "$portfile")" \
		"$RSYSLOG_OUT_LOG.expected" || error_exit 1
	wait_file_lines "$RSYSLOG_OUT_LOG" "$NUMMESSAGES"
	shutdown_when_empty
	wait_shutdown
	cmp_exact_file "$RSYSLOG_OUT_LOG.expected" "$RSYSLOG_OUT_LOG"
	case "$mode" in
		safe|mixed)
			content_count_check "parser: received legacy single-message" 1 "$diagnostics" ;;
		*)
			check_not_present "parser: received legacy single-message" "$diagnostics" ;;
	esac
	if [ "$mode" = mixed ]; then
		content_count_check "legacy z compression received without octet-counted TCP framing" 1 "$diagnostics"
	else
		check_not_present "legacy z compression received without octet-counted TCP framing" "$diagnostics"
	fi
done
exit_test
