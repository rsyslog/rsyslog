#!/bin/bash
# Validate omfile final-component symlink policy. The test covers the default
# strict/warn compatibility modes plus an explicit action override. The oracle
# is the symlink target content after synchronized shutdown, with warning text
# asserted through the configured omfile output rather than rsyslogd stderr.
. ${srcdir:=.}/diag.sh init
require_plugin imtcp

run_case() {
	local name="$1"
	local mode="$2"
	local action_extra="$3"
	local expect_target="$4"
	local expect_warning="$5"
	local link="${RSYSLOG_DYNNAME}.${name}.link"
	local target="${RSYSLOG_DYNNAME}.${name}.target"
	local normal="${RSYSLOG_DYNNAME}.${name}.normal"

	rm -f "$link" "$target" "$normal"
	: >"$target"
	ln -s "$target" "$link" || skip_test

	generate_conf
	add_conf '
global(compatibility.defaults.secure="'${mode}'")
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="0" listenPortFileName="'${RSYSLOG_DYNNAME}'.tcpflood_port")

template(name="outfmt" type="string" string="%msg:F,58:2%\n")
if $msg contains "msgnum:" then {
	action(type="omfile" template="outfmt" file="'${link}'" '${action_extra}')
}
action(type="omfile" file="'${normal}'" template="outfmt")
'
	startup
	injectmsg 0 1
	shutdown_when_empty
	wait_shutdown

	if [ "$expect_target" = "yes" ]; then
		content_check "00000000" "$target"
	else
		if [ -s "$target" ]; then
			echo "FAIL: ${name} wrote through symlink target unexpectedly"
			cat "$target"
			error_exit 1
		fi
	fi

	if [ "$expect_warning" = "yes" ]; then
		content_check "following symbolic link output file" "$normal"
	else
		check_not_present "following symbolic link output file" "$normal"
	fi
}

run_case strict strict '' no no
run_case warn warn '' yes yes
run_case explicit_on warn 'followSymlinks="on"' yes no
run_case explicit_off backward-compatible 'followSymlinks="off"' no no

exit_test
