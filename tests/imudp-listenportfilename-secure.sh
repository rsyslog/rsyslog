#!/bin/bash
# Regression test for imudp listenPortFileName path handling.  The listener
# writes its bound port before privilege drop, so the oracle is an internal
# diagnostic that rejects a symlink handoff path plus an unchanged symlink
# target. The timeout only bounds the foreground daemon after config activation.
. ${srcdir:=.}/diag.sh init

TARGET_FILE="${RSYSLOG_DYNNAME}.target"
PORT_FILE="${RSYSLOG_DYNNAME}.imudp_port"
printf 'must-stay-intact\n' > "$TARGET_FILE"
ln -s "$TARGET_FILE" "$PORT_FILE"

generate_conf
add_conf '
module(load="../plugins/imudp/.libs/imudp")
input(type="imudp" address="127.0.0.1" port="0" listenPortFileName="'$PORT_FILE'")
'

startup_common
set +e
timeout 3 ../tools/rsyslogd -C -n -i"${RSYSLOG_PIDBASE}.pid" -M"$RSYSLOG_MODDIR" -f"$CONF_FILE" \
    >"${RSYSLOG_DYNNAME}.startup.log" 2>&1
rc=$?
set -e
if [ "$rc" -eq 0 ]; then
    echo "FAIL: expected foreground rsyslogd to keep running or report an error after activation, rc=$rc"
    cat "${RSYSLOG_DYNNAME}.startup.log"
    error_exit 1
fi

printf 'must-stay-intact\n' > "${RSYSLOG_DYNNAME}.expected"
cmp_exact_file "${RSYSLOG_DYNNAME}.expected" "$TARGET_FILE"
content_check "listenPortFileName: refusing to write to non-regular file" "${RSYSLOG_DYNNAME}.startup.log"
exit_test
