#!/bin/bash
# Validate that list-template regex.match and regex.submatch values outside the
# fixed regexec pmatch[] range are rejected during RainerScript config parsing.
# Oracle: rsyslogd -N1 must fail for both configs and emit the specific range
# validation message, so the runtime never reaches out-of-bounds match access.
. ${srcdir:=.}/diag.sh init
modpath="../runtime/.libs:../.libs"

run_invalid_conf() {
    local name="$1"
    local config="$2"
    local expect="$3"
    local cfg="${RSYSLOG_DYNNAME}.${name}.conf"
    local log="${RSYSLOG_DYNNAME}.${name}.log"

    cat > "$cfg" <<RS_EOF
$config
RS_EOF

    ../tools/rsyslogd -N1 -f "$cfg" -M"$modpath" > "$log" 2>&1
    if [ $? -eq 0 ]; then
        echo "FAIL: config check unexpectedly succeeded for $name"
        cat "$log"
        error_exit 1
    fi

    content_check "$expect" "$log"
}

run_invalid_conf "bad-submatch" \
'template(name="bad_submatch" type="list") {
  property(name="msg" regex.expression="(a)" regex.submatch="-1")
}' \
"template bad_submatch error: regex.submatch=-1 is invalid (supported range 0..9)"

run_invalid_conf "bad-match" \
'template(name="bad_match" type="list") {
  property(name="msg" regex.expression="(a)" regex.match="10")
}' \
"template bad_match error: regex.match=10 is invalid (supported range 0..9)"

exit_test
