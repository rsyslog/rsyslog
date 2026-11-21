#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Shared helpers for the reload-report-*.sh tests (config.reloadOnHUP="report").
# Not a test by itself: it is sourced by the tests after "diag.sh init".
#
# The tests come in RainerScript and YAML flavours that must behave the same.
# RR_FRONTEND selects the flavour (default "rs"; the *-yaml.sh wrappers set
# "yaml"). In both flavours:
#   - the testbench preamble (imdiag, "rsyslogd" messages -> $RR_STARTED) is
#     RainerScript, because generate_conf writes it;
#   - the setting, the changeable include fragment ($RR_INC) and the include
#     glob directory ($RR_DIR) are RainerScript ("rs") or YAML ("yaml").
# Relative paths are used on purpose: rsyslogd chdir()s to "/" after loading,
# so this also covers that the config file record stores absolute paths.

RR_FRONTEND=${RR_FRONTEND:-rs}
RR_STARTED="${RSYSLOG_DYNNAME}.started" # rsyslogd's own messages, see generate_conf
RR_DIR="${RSYSLOG_DYNNAME}.d"
RR_STATS="${RSYSLOG_DYNNAME}.stats"
if [ "$RR_FRONTEND" = "yaml" ]; then
	# the *-yaml.sh twins are registered under HAVE_LIBYAML only
	RR_EXT=yaml
else
	RR_EXT=conf
fi
RR_INC="${RSYSLOG_DYNNAME}.inc.${RR_EXT}"
RR_REPORT='config reload check:'

# $1 = mode ("report", "off" or "" for "setting not given"), $2 = "stats" to
# also load impstats (statistics are written to $RR_STATS)
rr_setup() {
	local mode="$1"
	mkdir -p "$RR_DIR"
	rr_inc_valid 1
	generate_conf
	add_conf '
template(name="outfmt" type="string" string="%msg:F,58:2%\n")
:msg, contains, "msgnum:" action(type="omfile" template="outfmt" file="'$RSYSLOG_OUT_LOG'")
'
	if [ "$2" = "stats" ]; then
		add_conf '
module(load="../plugins/impstats/.libs/impstats" interval="1" resetCounters="off"
	log.file="'$RR_STATS'" log.syslog="off")
'
	fi
	if [ "$RR_FRONTEND" = "yaml" ]; then
		{
			printf 'global:\n'
			[ -n "$mode" ] && printf '  config.reloadOnHUP: "%s"\n' "$mode"
			printf '  janitor.interval: 10\n'
			printf 'include:\n  - path: "%s"\n  - path: "%s/*.yaml"\n' "$RR_INC" "$RR_DIR"
		} > "${RSYSLOG_DYNNAME}.top.yaml"
		add_conf 'include(file="'${RSYSLOG_DYNNAME}'.top.yaml")
'
	else
		[ -n "$mode" ] && add_conf 'global(config.reloadOnHUP="'$mode'")
'
		add_conf 'include(file="'$RR_INC'")
include(file="'$RR_DIR'/*.conf")
'
	fi
}

# Valid include fragment; $1 = revision (changes the content, not the validity).
rr_inc_valid() {
	if [ "$RR_FRONTEND" = "yaml" ]; then
		printf 'templates:\n  - name: rrtpl\n    type: string\n    string: "rev %s\\n"\n' "$1" > "$RR_INC"
	else
		printf '# revision %s\ntemplate(name="rrtpl" type="string" string="rev %s\\n")\n' "$1" "$1" > "$RR_INC"
	fi
}

# Include fragment with a syntax error.
rr_inc_invalid() {
	if [ "$RR_FRONTEND" = "yaml" ]; then
		printf 'templates: [ this is: not : valid\n' > "$RR_INC"
	else
		printf 'if $msg contains then {\n' > "$RR_INC"
	fi
}

# Add a valid file that matches the include glob (a file that did not exist at start).
rr_glob_add() {
	if [ "$RR_FRONTEND" = "yaml" ]; then
		printf 'templates:\n  - name: rrtpl2\n    type: string\n    string: "x"\n' > "$RR_DIR/add.yaml"
	else
		printf 'template(name="rrtpl2" type="string" string="x")\n' > "$RR_DIR/add.conf"
	fi
}

# The validation child can be held at a point of the test's choosing: the
# include fragment may define a lookup table whose file is a FIFO. Loading the
# table blocks the child in open() until the test opens the other end. (The
# running daemon never loads this fragment.)
RR_FIFO="${RSYSLOG_DYNNAME}.lookup.fifo"

# Include fragment that makes the child load $RR_FIFO.
rr_inc_block() {
	if [ "$RR_FRONTEND" = "yaml" ]; then
		printf 'lookup_tables:\n  - name: rrlt\n    file: "%s"\n' "$RR_FIFO" > "$RR_INC"
	else
		printf 'lookup_table(name="rrlt" file="%s")\n' "$RR_FIFO" > "$RR_INC"
	fi
}

# Valid lookup table content: written into the FIFO (releases a blocked child)
# or into a regular file of the same name.
rr_lookup_content() {
	printf '{"version":1,"nomatch":"none","type":"string","table":[{"index":"a","value":"b"}]}\n'
}

# $1 pattern, $2 file, $3 minimum number of matching lines. Bounded by TB_TEST_TIMEOUT.
rr_wait_count() {
	local deadline=$(( $(date +%s) + TB_TEST_TIMEOUT )) cnt=0
	while true; do
		[ -f "$2" ] && cnt=$(grep -c -F -- "$1" < "$2")
		[ "${cnt:-0}" -ge "$3" ] && return 0
		if [ "$(date +%s)" -ge "$deadline" ]; then
			printf 'FAIL: rr_wait_count: wanted %d lines with "%s" in %s, have %d\n' "$3" "$1" "$2" "$cnt"
			[ -f "$2" ] && cat -n "$2"
			error_exit 1
		fi
		$TESTTOOL_DIR/msleep 100
	done
}

# $1 pattern, $2 file, $3 exact number of matching lines
rr_assert_count() {
	local cnt=0
	[ -f "$2" ] && cnt=$(grep -c -F -- "$1" < "$2")
	if [ "${cnt:-0}" -ne "$3" ]; then
		printf 'FAIL: expected %d lines with "%s" in %s, have %d\n' "$3" "$1" "$2" "$cnt"
		[ -f "$2" ] && cat -n "$2"
		error_exit 1
	fi
}

# Wait until the last statistics line of the config.reload.check object has
# "$1=<value>" with value >= $2. Prints nothing; fails on timeout.
rr_wait_stat() {
	local deadline=$(( $(date +%s) + TB_TEST_TIMEOUT )) val=""
	while true; do
		val=$(rr_stat "$1")
		[ -n "$val" ] && [ "$val" -ge "$2" ] && return 0
		if [ "$(date +%s)" -ge "$deadline" ]; then
			printf 'FAIL: rr_wait_stat: wanted %s >= %s, have "%s"\n' "$1" "$2" "$val"
			[ -f "$RR_STATS" ] && grep 'config.reload.check' "$RR_STATS" | tail -n 3
			error_exit 1
		fi
		$TESTTOOL_DIR/msleep 200
	done
}

# print the current value of statistics counter $1 (empty if not yet known)
rr_stat() {
	[ -f "$RR_STATS" ] || return 0
	grep 'config.reload.check:' "$RR_STATS" | tail -n 1 | sed -n "s/.* $1=\([0-9][0-9]*\).*/\1/p"
}

# print the pids of all child processes of the daemon, zombies included (their
# cmdline is empty, so rr_child_pids cannot see them); reads /proc/PID/stat
rr_daemon_children() {
	local daemon p st
	daemon=$(cat "$RSYSLOG_PIDBASE.pid")
	for p in /proc/[0-9]*; do
		st=$(cat "$p/stat" 2>/dev/null) || continue
		st=${st##*) }	# drop "pid (comm) ": comm may contain blanks
		set -- $st	# $1 = state, $2 = ppid
		if [ "$2" = "$daemon" ]; then
			printf '%s(%s)\n' "${p#/proc/}" "$1"
		fi
	done
}

# print the pids of running validation children of this test (cmdline ends in -N1
# and names this test's config file); needs /proc
rr_child_pids() {
	local p
	for p in /proc/[0-9]*; do
		if tr '\0' ' ' < "$p/cmdline" 2>/dev/null | grep -q -- "${RSYSLOG_DYNNAME}.*-N1 \$"; then
			printf '%s\n' "${p#/proc/}"
		fi
	done
}

rr_wait_child() {
	local deadline=$(( $(date +%s) + TB_TEST_TIMEOUT ))
	while [ -z "$(rr_child_pids)" ]; do
		if [ "$(date +%s)" -ge "$deadline" ]; then
			printf 'FAIL: no validation child process appeared\n'
			error_exit 1
		fi
		$TESTTOOL_DIR/msleep 100
	done
}
