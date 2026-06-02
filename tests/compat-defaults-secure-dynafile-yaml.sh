#!/bin/bash
# Validate secure dynafile template defaults from YAML configuration.
. ${srcdir:=.}/diag.sh init
require_plugin imtcp

modpath="${RSYSLOG_MODDIR}"

run_expect_success() {
    local cfg="$1"
    local log="$2"
    ../tools/rsyslogd -C -N1 -M"${modpath}" -f"${cfg}" >"${log}" 2>&1
    local rc=$?
    if [ $rc -ne 0 ]; then
        echo "FAIL: expected ${cfg} to validate"
        cat "${log}"
        error_exit 1
    fi
}

write_yaml_dynafile_config() {
    local cfg="$1"
    local mode="$2"
    cat >"${cfg}" <<YAML_EOF
version: 2
global:
  compatibility.defaults.secure: "${mode}"
templates:
  - name: dynfile
    type: string
    string: "${RSYSLOG_DYNNAME}.warn_%msg:F,58:2%.log"
  - name: outfmt
    type: string
    string: "%msg:F,58:3%\\n"
rulesets:
  - name: main
    filter: ':msg, contains, "secure-default:"'
    actions:
      - type: omfile
        dynafile: dynfile
        template: outfmt
YAML_EOF
}

write_yaml_dynafile_config "${RSYSLOG_DYNNAME}.warn.yaml" "warn"
run_expect_success "${RSYSLOG_DYNNAME}.warn.yaml" "${RSYSLOG_DYNNAME}.warn.log"
content_check 'omfile dynafile template' "${RSYSLOG_DYNNAME}.warn.log"
content_check 'compatibility.defaults.secure="strict"' "${RSYSLOG_DYNNAME}.warn.log"

write_yaml_dynafile_config "${RSYSLOG_DYNNAME}.backward.yaml" "backward-compatible"
run_expect_success "${RSYSLOG_DYNNAME}.backward.yaml" "${RSYSLOG_DYNNAME}.backward.log"
if grep -q 'omfile dynafile template' "${RSYSLOG_DYNNAME}.backward.log"; then
    echo "FAIL: backward-compatible mode emitted a dynafile secure default warning"
    cat "${RSYSLOG_DYNNAME}.backward.log"
    error_exit 1
fi

cat >"${RSYSLOG_DYNNAME}.mixed.yaml" <<YAML_EOF
version: 2
global:
  compatibility.defaults.secure: "strict"
templates:
  - name: shared
    type: string
    string: "${RSYSLOG_DYNNAME}.mixed_%msg:F,58:2%.log"
rulesets:
  - name: main
    actions:
      - type: omfile
        dynafile: shared
        template: shared
      - type: omfile
        file: "${RSYSLOG_DYNNAME}.mixed.out"
        template: shared
YAML_EOF
run_expect_success "${RSYSLOG_DYNNAME}.mixed.yaml" "${RSYSLOG_DYNNAME}.mixed.log"
content_check 'used both as an omfile dynafile template and for other output' "${RSYSLOG_DYNNAME}.mixed.log"

generate_conf
add_conf '
include(file="'${RSYSLOG_DYNNAME}'.strict.yaml")
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="0" listenPortFileName="'${RSYSLOG_DYNNAME}'.tcpflood_port"
      ruleset="main")
'

cat >"${RSYSLOG_DYNNAME}.strict.yaml" <<YAML_EOF
version: 2
global:
  compatibility.defaults.secure: "strict"
templates:
  - name: dynfile
    type: string
    string: "${RSYSLOG_DYNNAME}.strict_%msg:F,58:2%.log"
  - name: dynfile-drop
    type: string
    string: "${RSYSLOG_DYNNAME}.drop_%msg:F,58:2:secpath-drop%.log"
  - name: outfmt
    type: string
    string: "%msg:F,58:3%\\n"
  - name: plainfmt
    type: string
    string: "%msg:F,58:2%\\n"
rulesets:
  - name: main
    filter: ':msg, contains, "secure-default:"'
    actions:
      - type: omfile
        dynafile: dynfile
        template: outfmt
      - type: omfile
        dynafile: dynfile-drop
        template: outfmt
      - type: omfile
        file: "${RSYSLOG_DYNNAME}.plain.out"
        template: plainfmt
YAML_EOF

startup
tcpflood -m1 -M "\"<129>Mar 10 01:00:00 172.20.245.8 tag secure-default:a/b:payload\""
shutdown_when_empty
wait_shutdown

printf 'payload\n' >"${RSYSLOG_DYNNAME}.expected"
cmp_exact_file "${RSYSLOG_DYNNAME}.expected" "${RSYSLOG_DYNNAME}.strict_a_b.log" || {
    echo "FAIL: strict mode did not replace slash in dynafile template"
    error_exit 1
}
cmp_exact_file "${RSYSLOG_DYNNAME}.expected" "${RSYSLOG_DYNNAME}.drop_ab.log" || {
    echo "FAIL: strict mode overrode explicit secpath-drop"
    error_exit 1
}
printf 'a/b\n' >"${RSYSLOG_DYNNAME}.plain.expected"
cmp_exact_file "${RSYSLOG_DYNNAME}.plain.expected" "${RSYSLOG_DYNNAME}.plain.out" || {
    echo "FAIL: strict mode changed a non-dynafile output template"
    error_exit 1
}

exit_test
