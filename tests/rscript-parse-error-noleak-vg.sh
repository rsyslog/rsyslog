#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# This file is part of the rsyslog project, released under ASL 2.0
#
# Intent: when the RainerScript parser rejects a config, the values it has
# built so far (objects, statements, expressions, name/value lists, arrays,
# function parameters, legacy lines, variable names) must be released, and
# error reporting at the end of the config must not read freed lexer memory.
# This matters for processes that parse configs repeatedly; at normal startup
# the process exits and leaks go unnoticed.
#
# Stimulus: "rsyslogd -N1" on several broken configs (syntax errors in the
# middle of objects, inside nested if/foreach, after legacy action lines and
# $-directives, in an unterminated config, a failing include() and a syntax
# error inside include(text=...)).
# Oracle: every run must still fail config validation with exit code 1
# (diagnostics and exit codes must be unchanged), and valgrind must report no
# definitely-lost memory and no memory errors. Valgrind signals those via
# --error-exitcode=10 (leak kind restricted to "definite"; still-reachable
# memory is irrelevant here). No timing is involved: each run is synchronous.
. ${srcdir:=.}/diag.sh init

CASE_NR=0

# $1 = case description, config text is read from stdin
run_case() {
	CASE_NR=$((CASE_NR+1))
	local conf="${RSYSLOG_DYNNAME}.case$CASE_NR.conf"
	local vglog="${RSYSLOG_DYNNAME}.case$CASE_NR.vg.log"
	cat > "$conf"
	valgrind --error-exitcode=10 --leak-check=full --errors-for-leak-kinds=definite \
		--show-leak-kinds=definite --malloc-fill=ff --free-fill=fe \
		--suppressions=$srcdir/known_issues.supp ${EXTRA_VALGRIND_SUPPRESSIONS:-} \
		--log-file="$vglog" \
		../tools/rsyslogd -N1 -f"$conf" -M"$RSYSLOG_MODDIR" \
		>"${RSYSLOG_DYNNAME}.case$CASE_NR.out" 2>&1
	local rc=$?
	if [ $rc -ne 1 ]; then
		echo "FAIL: case $CASE_NR ($1): rsyslogd -N1 exit code $rc, expected 1 (10 means valgrind found leaks/errors)"
		cat "${RSYSLOG_DYNNAME}.case$CASE_NR.out"
		cat "$vglog"
		error_exit 1
	fi
	if ! grep -q "could not interpret master config file\|error during parsing" \
		"${RSYSLOG_DYNNAME}.case$CASE_NR.out"; then
		echo "FAIL: case $CASE_NR ($1): expected parse error diagnostic is missing"
		cat "${RSYSLOG_DYNNAME}.case$CASE_NR.out"
		error_exit 1
	fi
}

run_case "syntax error mid-object" <<'EOF'
template(name="t1" type="string" string="%msg%\n")
template(name="t2" type="list" constant(value="a" ] )
action(type="omfile" file="/tmp/x")
EOF

run_case "error inside nested if/foreach" <<'EOF'
ruleset(name="r") {
  if $msg contains "a" then {
    foreach ($.i in $!arr) do {
      set $.x = $.i & "x" +;
    }
  }
}
EOF

run_case "error in else-if chain" <<'EOF'
if $msg contains ["a","b"] then {
  action(type="omfile" file="/tmp/y")
} else if $syslogtag == "x" then
  set $!a = field($msg, 32, 1) )
EOF

run_case "error after legacy action lines" <<'EOF'
*.* /tmp/legacy.log
$template foo,"%msg%\n"
mail.* /tmp/mail.log
if ( then
EOF

run_case "error after property filter and legacy action" <<'EOF'
:msg, contains, "x" /tmp/z.log
*.* /tmp/legacy2.log
auth.info { action(type="omfile" file="/tmp/q") )
EOF

run_case "unterminated config at end of file" <<'EOF'
template(name="t" type="list") { property(name="msg" ) constant(value="a" ] }
unset $!x
set $!x = exists($!y) and ( ;
EOF

run_case "error in function parameter list" <<'EOF'
reload_lookup_table("a", ) ;
EOF

run_case "error in array and legacy action inside ruleset" <<'EOF'
ruleset(name="x") { prog.* /tmp/l  & stop  set $!a = [ "a", ; }
EOF

run_case "error in prifilter block" <<'EOF'
:msg, contains, "x" { set $!a = ( "x" ;
EOF

run_case "legacy ruleset, BSD blocks and incomplete call" <<'EOF'
$ruleset foo
*.* /tmp/l1
!tag
+host
:msg, contains, "x" stop
if $msg == "a" then call
EOF

run_case "failing include" <<'EOF'
include(file="/nonexistent/file.conf")
action(type="omfile" file="/tmp/a")
EOF

run_case "failing include with abort-if-missing" <<'EOF'
include(file="/nonexistent/file.conf" mode="abort-if-missing")
action(type="omfile" file="/tmp/a")
EOF

run_case "syntax error inside include text" <<'EOF'
include(file="/nonexistent/*.conf" mode="optional")
include(text="set $!x = ;")
action(type="omfile" file="/tmp/a")
EOF

exit_test
