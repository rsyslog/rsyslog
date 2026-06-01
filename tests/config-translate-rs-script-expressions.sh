#!/bin/bash
# Check canonical RainerScript emission for complex script statements.
#
# This is a cheap -N1 translation test for runtime/translate.c's script
# serializer. It covers escaped strings, arrays, unary minus, set/reset/unset,
# boolean/comparison expressions, exists(), foreach, call, call_indirect, and
# if/else block formatting. The translated RainerScript is validated again so
# the exact-output oracle proves the emitted config remains parseable.
#
# Part of the testbench for rsyslog.
#
# This file is part of rsyslog.
# Released under ASL 2.0
. ${srcdir:=.}/diag.sh init
modpath="../runtime/.libs:../.libs"
target_out="/tmp/${RSYSLOG_DYNNAME}.target.log"

cat > "${RSYSLOG_DYNNAME}.conf" <<'RS_EOF'
ruleset(name="target") {
  action(type="omfile" file="@TARGET_OUT@")
}

ruleset(name="main") {
  set $.n = -7;
  set $.s = "line\nquote\"tab\tbackslash\\";
  set $.arr = ["one", "two"];
  reset $.scratch = $.arr;
  unset $.arr;
  if not exists($!missing) and (($msg contains_i "ERR") or ($msg startswith " start")) then {
    foreach ($.item in $!items) do {
      call target
    }
  } else {
    call_indirect "tar" & "get";
  }
}
RS_EOF
sed -i "s|@TARGET_OUT@|${target_out}|g" "${RSYSLOG_DYNNAME}.conf"

../tools/rsyslogd -N1 -f "${RSYSLOG_DYNNAME}.conf" -F rainerscript -o "${RSYSLOG_DYNNAME}.out.conf" -M"$modpath" ||
	error_exit $?

cat > "${RSYSLOG_DYNNAME}.expected.conf" <<'RS_EOF'
ruleset(name="target") {
  action(type="omfile" file="@TARGET_OUT@")
}

ruleset(name="main") {
  set $.n = -7;
  set $.s = "line\nquote\"tab\tbackslash\\";
  set $.arr = ["one", "two"];
  reset $.scratch = $.arr;
  unset $.arr;
  if (not exists($!missing) and (($msg contains_i "ERR") or ($msg startswith " start"))) then {
    foreach ($.item in $!items) do {
      call target
    }
  } else {
    call_indirect ("tar" & "get");
  }
}

RS_EOF
sed -i "s|@TARGET_OUT@|${target_out}|g" "${RSYSLOG_DYNNAME}.expected.conf"
cmp_exact_file "${RSYSLOG_DYNNAME}.expected.conf" "${RSYSLOG_DYNNAME}.out.conf"

../tools/rsyslogd -N1 -f "${RSYSLOG_DYNNAME}.out.conf" -M"$modpath" || error_exit $?

echo SUCCESS: RainerScript script expression translation
exit_test
