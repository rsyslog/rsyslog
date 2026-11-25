#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
#
# Copyright 2026 Rainer Gerhards and Adiscon GmbH.
#
# A dynafile template without a fixed leading directory renders relative
# names. Without -C, rsyslogd works from the filesystem root, where such a
# name can select any location; the fallback guard must reject it, warn at
# startup and log the rejection only once. With -C the same name stays below
# the startup directory and must still be accepted.
# The testbench always uses -C, so this test starts rsyslogd itself. It needs
# no network input: the daemon's own startup message drives the action.
. ${srcdir:=.}/diag.sh init

export REL_DIR="tmp-${RSYSLOG_DYNNAME}"
export REL_NAME="${REL_DIR}/relative-root.log"
export ESCAPE_OUT="/${REL_NAME}"
export CWD_OUT="$(pwd)/${REL_NAME}"
export CONF="$(pwd)/${RSYSLOG_DYNNAME}.relroot.conf"
export CTL_OUT="$(pwd)/${RSYSLOG_DYNNAME}.relroot.ctl"
export PIDFILE="$(pwd)/${RSYSLOG_DYNNAME}.relroot.pid"

cleanup_relroot() {
	rm -rf "/${REL_DIR}" "$(pwd)/${REL_DIR}" "$CONF" "$CTL_OUT" "$PIDFILE"
}
cleanup_relroot

# REL_DIR does not exist below /, and an unprivileged test cannot create it
# there; the escape check below therefore also detects a successful create.
cat > "$CONF" <<CONF_EOF
template(name="dynfile" type="string" string="%\$!f%")
template(name="outfmt" type="string" string="%msg%\n")
template(name="ctlfmt" type="string" string="%msg%\n")
action(type="omfile" file="$CTL_OUT" template="ctlfmt")
if \$msg contains "start" and not (\$msg contains "blocked") then {
	set \$!f = "$REL_NAME";
	action(type="omfile" dynafile="dynfile" template="outfmt" createDirs="on")
}
CONF_EOF

# $1: extra rsyslogd option ("" or "-C"); $2: shell condition that marks
# completion. Fails the test if rsyslogd exits early or the condition does
# not become true within 10 seconds.
run_daemon() {
	local i done=0
	rm -f "$CTL_OUT" "$PIDFILE"
	../tools/rsyslogd $1 -n -M"$RSYSLOG_MODDIR" -f"$CONF" -i"$PIDFILE" &
	local pid=$!
	for i in $(seq 1 100); do
		if eval "$2"; then
			done=1
			break
		fi
		if ! kill -0 "$pid" 2>/dev/null; then
			echo "FAIL: rsyslogd $1 exited early"
			cat "$CTL_OUT" 2>/dev/null
			cleanup_relroot
			error_exit 1
		fi
		$TESTTOOL_DIR/msleep 100
	done
	kill "$pid"
	wait "$pid"
	if [ $done -ne 1 ]; then
		echo "FAIL: rsyslogd $1 did not reach: $2"
		cat "$CTL_OUT" 2>/dev/null
		cleanup_relroot
		error_exit 1
	fi
}

run_daemon "" "grep -q 'would resolve against the filesystem root' \"\$CTL_OUT\" 2>/dev/null"
if [ -e "$ESCAPE_OUT" ]; then
	echo "FAIL: relative dynafile name resolved against / and created: $ESCAPE_OUT"
	cleanup_relroot
	error_exit 1
fi
if ! grep -q "dynafile template 'dynfile' has no fixed leading directory" "$CTL_OUT"; then
	echo "FAIL: expected startup warning for template without fixed directory"
	cat "$CTL_OUT"
	cleanup_relroot
	error_exit 1
fi
if [ "$(grep -c 'would resolve against the filesystem root' "$CTL_OUT")" -ne 1 ]; then
	echo "FAIL: relative-root rejection was not logged exactly once"
	cat "$CTL_OUT"
	cleanup_relroot
	error_exit 1
fi

run_daemon "-C" "[ -f \"\$CWD_OUT\" ]"
if [ -e "$ESCAPE_OUT" ]; then
	echo "FAIL: relative dynafile name with -C escaped to: $ESCAPE_OUT"
	cleanup_relroot
	error_exit 1
fi

cleanup_relroot
exit_test
