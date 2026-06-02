#!/bin/bash
# Validate RainerScript parity for secure dynafile template defaults.
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

cat >"${RSYSLOG_DYNNAME}.mixed.conf" <<CONF_EOF
global(compatibility.defaults.secure="strict")
template(name="shared" type="string" string="${RSYSLOG_DYNNAME}.mixed_%msg:F,58:2%.log")
action(type="omfile" dynafile="shared" template="shared")
action(type="omfile" file="${RSYSLOG_DYNNAME}.mixed.out" template="shared")
CONF_EOF
run_expect_success "${RSYSLOG_DYNNAME}.mixed.conf" "${RSYSLOG_DYNNAME}.mixed.log"
content_check 'used both as an omfile dynafile template and for other output' "${RSYSLOG_DYNNAME}.mixed.log"

generate_conf
add_conf '
global(compatibility.defaults.secure="strict")
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="0" listenPortFileName="'${RSYSLOG_DYNNAME}'.tcpflood_port")

template(name="dynfile" type="string" string="'${RSYSLOG_DYNNAME}'.rs_%msg:F,58:2%.log")
template(name="outfmt" type="string" string="%msg:F,58:3%\n")

:msg, contains, "secure-default:" action(type="omfile" dynafile="dynfile" template="outfmt")
'

startup
tcpflood -m1 -M "\"<129>Mar 10 01:00:00 172.20.245.8 tag secure-default:a/b:payload\""
shutdown_when_empty
wait_shutdown

printf 'payload\n' >"${RSYSLOG_DYNNAME}.expected"
cmp_exact_file "${RSYSLOG_DYNNAME}.expected" "${RSYSLOG_DYNNAME}.rs_a_b.log" || {
    echo "FAIL: strict mode did not replace slash in RainerScript dynafile template"
    error_exit 1
}

exit_test
