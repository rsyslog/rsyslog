#!/usr/bin/env bash
# Regression test for issue #6633: failed setuid()/setgroups() calls during
# privilege drop must produce clear diagnostics with the requested id and missing
# capability context. The oracle is startup failure with those diagnostics on
# stderr; the test runs only as non-root so privilege changes fail with EPERM.
. ${srcdir:=.}/diag.sh init

if [ "${EUID}" -eq 0 ]; then
    echo "Skipping: this test needs to run as non-root so privilege dropping fails with EPERM."
    exit 77
fi

current_gid="$(id -g)"
target_uid=""
for user in daemon syslog nobody; do
    uid="$(id -u "$user" 2>/dev/null || true)"
    if [ -n "$uid" ] && [ "$uid" -ne "${EUID}" ] && [ "$uid" -ne 0 ]; then
        target_uid="$uid"
        break
    fi
done
if [ -z "$target_uid" ]; then
    target_uid=$((EUID + 1))
fi

target_gid=""
for group in daemon syslog nogroup nobody; do
    gid="$(getent group "$group" 2>/dev/null | cut -d: -f3 || true)"
    if [ -n "$gid" ] && [ "$gid" -ne "${current_gid}" ] && [ "$gid" -ne 0 ]; then
        target_gid="$gid"
        break
    fi
done
if [ -z "$target_gid" ]; then
    target_gid=$((current_gid + 1))
fi

generate_conf
add_conf '
global(
    errormessagestostderr.maxnumber="20"
    privdrop.user.id="'${target_uid}'"
)

template(name="outfmt" type="string" string="%msg%\n")
action(type="omfile" file="'${RSYSLOG_OUT_LOG}'" template="outfmt")
'

../tools/rsyslogd -n -iNONE -f"${TESTCONF_NM}.conf" -M"$RSYSLOG_MODDIR" >"${RSYSLOG_DYNNAME}.log" 2>&1
if [ $? -eq 0 ]; then
    echo "FAIL: expected startup failure when setuid() lacks permission"
    cat "${RSYSLOG_DYNNAME}.log"
    error_exit 1
fi

grep -F "could not set requested user id ${target_uid} via setuid()" "${RSYSLOG_DYNNAME}.log" >/dev/null || {
    echo "FAIL: expected setuid() privilege-drop error message"
    cat "${RSYSLOG_DYNNAME}.log"
    error_exit 1
}

grep -F "missing CAP_SETUID or insufficient privilege" "${RSYSLOG_DYNNAME}.log" >/dev/null || {
    echo "FAIL: expected CAP_SETUID context in privilege-drop error message"
    cat "${RSYSLOG_DYNNAME}.log"
    error_exit 1
}

generate_conf 2
add_conf '
global(
    errormessagestostderr.maxnumber="20"
    privdrop.group.id="'${target_gid}'"
)

template(name="outfmt" type="string" string="%msg%\n")
action(type="omfile" file="'${RSYSLOG_OUT_LOG}'" template="outfmt")
' 2

../tools/rsyslogd -n -iNONE -f"${TESTCONF_NM}2.conf" -M"$RSYSLOG_MODDIR" >"${RSYSLOG_DYNNAME}.setgid.log" 2>&1
if [ $? -eq 0 ]; then
    echo "FAIL: expected startup failure when setgroups()/setgid() lacks permission"
    cat "${RSYSLOG_DYNNAME}.setgid.log"
    error_exit 1
fi

grep -F "missing CAP_SETGID or insufficient privilege" "${RSYSLOG_DYNNAME}.setgid.log" >/dev/null || {
    echo "FAIL: expected CAP_SETGID context in privilege-drop error message"
    cat "${RSYSLOG_DYNNAME}.setgid.log"
    error_exit 1
}

grep -E "setgroups\\(\\)|setgid\\(\\)" "${RSYSLOG_DYNNAME}.setgid.log" >/dev/null || {
    echo "FAIL: expected setgroups() or setgid() privilege-drop error message"
    cat "${RSYSLOG_DYNNAME}.setgid.log"
    error_exit 1
}

exit_test
