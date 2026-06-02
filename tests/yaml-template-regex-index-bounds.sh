#!/bin/bash
# Validate that YAML list-template regex.match and regex.submatch values outside
# the fixed regexec pmatch[] range are rejected during config parsing.
# Oracle: rsyslogd -N1 must fail for both YAML configs and log the same range
# validation message as RainerScript, keeping both frontends in sync.
. ${srcdir:=.}/diag.sh init
require_yaml_support
modpath="../runtime/.libs:../.libs"

run_invalid_yaml() {
    local name="$1"
    local config="$2"
    local expect="$3"
    local cfg="${RSYSLOG_DYNNAME}.${name}.yaml"
    local log="${RSYSLOG_DYNNAME}.${name}.log"

    cat > "$cfg" <<YAML_EOF
$config
YAML_EOF

    ../tools/rsyslogd -N1 -f "$cfg" -M"$modpath" > "$log" 2>&1
    if [ $? -eq 0 ]; then
        echo "FAIL: YAML config check unexpectedly succeeded for $name"
        cat "$log"
        error_exit 1
    fi

    content_check "$expect" "$log"
}

run_invalid_yaml "bad-submatch" \
'version: 2
templates:
  - name: bad_submatch
    type: list
    elements:
      - property:
          name: msg
          regex.expression: "(a)"
          regex.submatch: "-1"' \
"template bad_submatch error: regex.submatch=-1 is invalid (supported range 0..9)"

run_invalid_yaml "bad-match" \
'version: 2
templates:
  - name: bad_match
    type: list
    elements:
      - property:
          name: msg
          regex.expression: "(a)"
          regex.match: "10"' \
"template bad_match error: regex.match=10 is invalid (supported range 0..9)"

exit_test
