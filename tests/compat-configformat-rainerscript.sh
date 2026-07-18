#!/bin/bash
# Validate RainerScript parity for compatibility.* global options.
. ${srcdir:=.}/diag.sh init

modpath="${RSYSLOG_MODDIR}"
have_imtcp_module=0
if ls ../plugins/imtcp/.libs/imtcp.* >/dev/null 2>&1; then
    have_imtcp_module=1
fi

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

run_expect_failure() {
    local cfg="$1"
    local log="$2"
    ../tools/rsyslogd -C -N1 -M"${modpath}" -f"${cfg}" >"${log}" 2>&1
    local rc=$?
    if [ $rc -eq 0 ]; then
        echo "FAIL: expected ${cfg} to fail validation"
        cat "${log}"
        error_exit 1
    fi
}

cat >"${RSYSLOG_DYNNAME}.valid.conf" <<CONF_EOF
global(
  compatibility.configformat.legacy="enable"
  compatibility.configformat.syslogd="enable"
  compatibility.configformat.property="enable"
  compatibility.defaults.secure="backward-compatible"
)
action(type="omfile" file="${RSYSLOG_DYNNAME}.out")
CONF_EOF
run_expect_success "${RSYSLOG_DYNNAME}.valid.conf" "${RSYSLOG_DYNNAME}.valid.log"

cat >"${RSYSLOG_DYNNAME}.secure-default.conf" <<CONF_EOF
global(compatibility.configformat.legacy="enable")
action(type="omfile" file="${RSYSLOG_DYNNAME}.out")
CONF_EOF
run_expect_success "${RSYSLOG_DYNNAME}.secure-default.conf" "${RSYSLOG_DYNNAME}.secure-default.log"
content_check 'backward-compatible insecure default in use' "${RSYSLOG_DYNNAME}.secure-default.log"

cat >"${RSYSLOG_DYNNAME}.legacy-warn.conf" <<CONF_EOF
global(compatibility.configformat.legacy="warn")
\$MainMsgQueueTimeoutShutdown 10000
action(type="omfile" file="${RSYSLOG_DYNNAME}.out")
CONF_EOF
run_expect_success "${RSYSLOG_DYNNAME}.legacy-warn.conf" "${RSYSLOG_DYNNAME}.legacy-warn.log"
content_check 'obsolete $-directive syntax encountered' "${RSYSLOG_DYNNAME}.legacy-warn.log"

cat >"${RSYSLOG_DYNNAME}.legacy-disable.conf" <<CONF_EOF
global(compatibility.configformat.legacy="disable")
\$MainMsgQueueTimeoutShutdown 10000
action(type="omfile" file="${RSYSLOG_DYNNAME}.out")
CONF_EOF
run_expect_failure "${RSYSLOG_DYNNAME}.legacy-disable.conf" "${RSYSLOG_DYNNAME}.legacy-disable.log"
content_check 'global(compatibility.configformat.legacy="enable")' "${RSYSLOG_DYNNAME}.legacy-disable.log"

cat >"${RSYSLOG_DYNNAME}.syslogd-warn.conf" <<CONF_EOF
global(compatibility.configformat.syslogd="warn")
mail.* action(type="omfile" file="${RSYSLOG_DYNNAME}.out")
CONF_EOF
run_expect_success "${RSYSLOG_DYNNAME}.syslogd-warn.conf" "${RSYSLOG_DYNNAME}.syslogd-warn.log"
content_check 'obsolete classic syslogd filter syntax encountered' "${RSYSLOG_DYNNAME}.syslogd-warn.log"

cat >"${RSYSLOG_DYNNAME}.syslogd-disable.conf" <<CONF_EOF
global(compatibility.configformat.syslogd="disable")
mail.* action(type="omfile" file="${RSYSLOG_DYNNAME}.out")
action(type="omfile" file="${RSYSLOG_DYNNAME}.out2")
CONF_EOF
run_expect_failure "${RSYSLOG_DYNNAME}.syslogd-disable.conf" "${RSYSLOG_DYNNAME}.syslogd-disable.log"
content_check 'global(compatibility.configformat.syslogd="enable")' "${RSYSLOG_DYNNAME}.syslogd-disable.log"

cat >"${RSYSLOG_DYNNAME}.property-warn.conf" <<CONF_EOF
global(compatibility.configformat.property="warn")
:msg, contains, "hello" action(type="omfile" file="${RSYSLOG_DYNNAME}.out")
CONF_EOF
run_expect_success "${RSYSLOG_DYNNAME}.property-warn.conf" "${RSYSLOG_DYNNAME}.property-warn.log"
content_check 'obsolete classic property-based filter syntax encountered' "${RSYSLOG_DYNNAME}.property-warn.log"

cat >"${RSYSLOG_DYNNAME}.omfwd-tls-driver-warn.conf" <<CONF_EOF
global(compatibility.defaults.secure="warn")
action(type="omfwd" target="127.0.0.1" protocol="tcp" streamdriver.name="gtls")
CONF_EOF
run_expect_success "${RSYSLOG_DYNNAME}.omfwd-tls-driver-warn.conf" "${RSYSLOG_DYNNAME}.omfwd-tls-driver-warn.log"
content_check 'omfwd has TLS-related settings but streamdriver.mode="0"; mode 0 uses plain TCP so TLS is not active' \
    "${RSYSLOG_DYNNAME}.omfwd-tls-driver-warn.log"

cat >"${RSYSLOG_DYNNAME}.omfwd-global-driver-warn.conf" <<CONF_EOF
global(compatibility.defaults.secure="warn" defaultNetstreamDriver="gtls")
action(type="omfwd" target="127.0.0.1" protocol="tcp")
CONF_EOF
run_expect_success "${RSYSLOG_DYNNAME}.omfwd-global-driver-warn.conf" "${RSYSLOG_DYNNAME}.omfwd-global-driver-warn.log"
content_check 'omfwd has TLS-related settings but streamdriver.mode="0"; mode 0 uses plain TCP so TLS is not active' \
    "${RSYSLOG_DYNNAME}.omfwd-global-driver-warn.log"

cat >"${RSYSLOG_DYNNAME}.omfwd-plain-implicit-mode0.conf" <<CONF_EOF
global(compatibility.defaults.secure="warn")
action(type="omfwd" target="127.0.0.1" protocol="tcp")
CONF_EOF
run_expect_success "${RSYSLOG_DYNNAME}.omfwd-plain-implicit-mode0.conf" \
    "${RSYSLOG_DYNNAME}.omfwd-plain-implicit-mode0.log"
content_check 'omfwd action uses protocol="tcp" with streamdriver.mode="0"' \
    "${RSYSLOG_DYNNAME}.omfwd-plain-implicit-mode0.log"

cat >"${RSYSLOG_DYNNAME}.omfwd-plain-explicit-mode0.conf" <<CONF_EOF
global(compatibility.defaults.secure="warn")
action(type="omfwd" target="127.0.0.1" protocol="tcp" streamdriver.mode="0")
CONF_EOF
run_expect_success "${RSYSLOG_DYNNAME}.omfwd-plain-explicit-mode0.conf" \
    "${RSYSLOG_DYNNAME}.omfwd-plain-explicit-mode0.log"
check_not_present 'omfwd action uses protocol="tcp" with streamdriver.mode="0"' \
    "${RSYSLOG_DYNNAME}.omfwd-plain-explicit-mode0.log"

cat >"${RSYSLOG_DYNNAME}.omfwd-strict-promote.conf" <<CONF_EOF
global(compatibility.defaults.secure="strict")
action(type="omfwd" targetSrv="_syslog._tcp.example.invalid" protocol="tcp" streamdriver.name="gtls")
CONF_EOF
run_expect_failure "${RSYSLOG_DYNNAME}.omfwd-strict-promote.conf" "${RSYSLOG_DYNNAME}.omfwd-strict-promote.log"
content_check 'targetSrv with TLS requires streamdriverpermittedpeers' "${RSYSLOG_DYNNAME}.omfwd-strict-promote.log"

cat >"${RSYSLOG_DYNNAME}.omfwd-strict-explicit-mode0.conf" <<CONF_EOF
global(compatibility.defaults.secure="strict")
action(type="omfwd" target="127.0.0.1" protocol="tcp" streamdriver.name="gtls" streamdriver.mode="0")
CONF_EOF
run_expect_failure "${RSYSLOG_DYNNAME}.omfwd-strict-explicit-mode0.conf" \
    "${RSYSLOG_DYNNAME}.omfwd-strict-explicit-mode0.log"
content_check 'omfwd: compatibility.defaults.secure="strict" rejects explicit streamdriver.mode="0"' \
    "${RSYSLOG_DYNNAME}.omfwd-strict-explicit-mode0.log"

cat >"${RSYSLOG_DYNNAME}.omfwd-legacy-explicit-mode0.conf" <<CONF_EOF
global(compatibility.defaults.secure="strict" compatibility.configformat.legacy="enable")
\$ActionSendStreamDriver gtls
\$ActionSendStreamDriverMode 0
*.* @@127.0.0.1:6514
CONF_EOF
run_expect_failure "${RSYSLOG_DYNNAME}.omfwd-legacy-explicit-mode0.conf" \
    "${RSYSLOG_DYNNAME}.omfwd-legacy-explicit-mode0.log"
content_check 'omfwd: compatibility.defaults.secure="strict" rejects explicit streamdriver.mode="0"' \
    "${RSYSLOG_DYNNAME}.omfwd-legacy-explicit-mode0.log"

cat >"${RSYSLOG_DYNNAME}.omfwd-backcompat-global-driver.conf" <<CONF_EOF
global(compatibility.defaults.secure="backward-compatible" defaultNetstreamDriver="gtls")
action(type="omfwd" target="127.0.0.1" protocol="tcp")
CONF_EOF
run_expect_success "${RSYSLOG_DYNNAME}.omfwd-backcompat-global-driver.conf" \
    "${RSYSLOG_DYNNAME}.omfwd-backcompat-global-driver.log"
check_not_present 'TLS is not active' "${RSYSLOG_DYNNAME}.omfwd-backcompat-global-driver.log"

if [ ${have_imtcp_module} -eq 1 ]; then
    cat >"${RSYSLOG_DYNNAME}.tls-warn.conf" <<CONF_EOF
global(compatibility.defaults.secure="warn")
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" address="127.0.0.1" port="0" listenPortFileName="${RSYSLOG_DYNNAME}.tlswarn.port" streamdriver.mode="0"
      streamdriver.name="gtls" streamdriver.authmode="x509/name")
action(type="omfwd" target="127.0.0.1" protocol="udp")
CONF_EOF
    run_expect_success "${RSYSLOG_DYNNAME}.tls-warn.conf" "${RSYSLOG_DYNNAME}.tls-warn.log"
    content_check 'imtcp has TLS-related settings but streamdriver.mode="0"; mode 0 uses plain TCP so TLS is not active' "${RSYSLOG_DYNNAME}.tls-warn.log"
    content_check 'omfwd action uses protocol="udp" (without TLS)' "${RSYSLOG_DYNNAME}.tls-warn.log"

    cat >"${RSYSLOG_DYNNAME}.imtcp-global-driver-warn.conf" <<CONF_EOF
global(compatibility.defaults.secure="warn" defaultNetstreamDriver="gtls")
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" address="127.0.0.1" port="0" listenPortFileName="${RSYSLOG_DYNNAME}.imtcp-global-driver-warn.port")
action(type="omfile" file="${RSYSLOG_DYNNAME}.out")
CONF_EOF
    run_expect_success "${RSYSLOG_DYNNAME}.imtcp-global-driver-warn.conf" \
        "${RSYSLOG_DYNNAME}.imtcp-global-driver-warn.log"
    content_check 'imtcp has TLS-related settings but streamdriver.mode="0"; mode 0 uses plain TCP so TLS is not active' \
        "${RSYSLOG_DYNNAME}.imtcp-global-driver-warn.log"

    cat >"${RSYSLOG_DYNNAME}.imtcp-plain-implicit-mode0.conf" <<CONF_EOF
global(compatibility.defaults.secure="warn")
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" address="127.0.0.1" port="0" listenPortFileName="${RSYSLOG_DYNNAME}.imtcp-plain-implicit-mode0.port")
action(type="omfile" file="${RSYSLOG_DYNNAME}.out")
CONF_EOF
    run_expect_success "${RSYSLOG_DYNNAME}.imtcp-plain-implicit-mode0.conf" \
        "${RSYSLOG_DYNNAME}.imtcp-plain-implicit-mode0.log"
    content_check 'imtcp input uses streamdriver.mode="0" (plain TCP without TLS)' \
        "${RSYSLOG_DYNNAME}.imtcp-plain-implicit-mode0.log"

    # explicit module-level mode 0 acknowledges plain TCP; instances inherit
    # both the mode and its explicitness flag, so no warning at either level
    cat >"${RSYSLOG_DYNNAME}.imtcp-plain-explicit-mode0.conf" <<CONF_EOF
global(compatibility.defaults.secure="warn")
module(load="../plugins/imtcp/.libs/imtcp" streamdriver.mode="0")
input(type="imtcp" address="127.0.0.1" port="0" listenPortFileName="${RSYSLOG_DYNNAME}.imtcp-plain-explicit-mode0.port")
action(type="omfile" file="${RSYSLOG_DYNNAME}.out")
CONF_EOF
    run_expect_success "${RSYSLOG_DYNNAME}.imtcp-plain-explicit-mode0.conf" \
        "${RSYSLOG_DYNNAME}.imtcp-plain-explicit-mode0.log"
    check_not_present 'imtcp input uses streamdriver.mode="0"' \
        "${RSYSLOG_DYNNAME}.imtcp-plain-explicit-mode0.log"

    cat >"${RSYSLOG_DYNNAME}.imtcp-strict-explicit-mode0.conf" <<CONF_EOF
global(compatibility.defaults.secure="strict" defaultNetstreamDriver="gtls")
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" address="127.0.0.1" port="0" listenPortFileName="${RSYSLOG_DYNNAME}.imtcp-strict-explicit-mode0.port"
      streamdriver.mode="0")
action(type="omfile" file="${RSYSLOG_DYNNAME}.out")
CONF_EOF
    run_expect_failure "${RSYSLOG_DYNNAME}.imtcp-strict-explicit-mode0.conf" \
        "${RSYSLOG_DYNNAME}.imtcp-strict-explicit-mode0.log"
    content_check 'imtcp input: compatibility.defaults.secure="strict" rejects explicit streamdriver.mode="0"' \
        "${RSYSLOG_DYNNAME}.imtcp-strict-explicit-mode0.log"

    cat >"${RSYSLOG_DYNNAME}.tls-anon-warn.conf" <<CONF_EOF
global(compatibility.defaults.secure="warn")
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" address="127.0.0.1" port="0" listenPortFileName="${RSYSLOG_DYNNAME}.tlsanon.port" streamdriver.mode="1"
      streamdriver.name="gtls" streamdriver.authmode="anon")
action(type="omfwd" target="127.0.0.1" protocol="tcp" streamdriver.mode="1" streamdriver.authmode="anon")
CONF_EOF
    run_expect_success "${RSYSLOG_DYNNAME}.tls-anon-warn.conf" "${RSYSLOG_DYNNAME}.tls-anon-warn.log"
    content_check 'imtcp uses streamdriver.authmode="anon"; server identity is not authenticated, so MITM is possible' "${RSYSLOG_DYNNAME}.tls-anon-warn.log"
    content_check 'omfwd uses streamdriver.authmode="anon"; server identity is not authenticated, so MITM is possible' "${RSYSLOG_DYNNAME}.tls-anon-warn.log"
fi

cat >"${RSYSLOG_DYNNAME}.invalid.conf" <<CONF_EOF
global(compatibility.configformat.legacy="bad")
action(type="omfile" file="${RSYSLOG_DYNNAME}.out")
CONF_EOF
run_expect_failure "${RSYSLOG_DYNNAME}.invalid.conf" "${RSYSLOG_DYNNAME}.invalid.log"
content_check 'compatibility.configformat.legacy' "${RSYSLOG_DYNNAME}.invalid.log"

cat >"${RSYSLOG_DYNNAME}.strict-dirty.conf" <<CONF_EOF
global(compatibility.defaults.secure="strict")
global(typo.parameter="bad")
action(type="omfile" file="${RSYSLOG_DYNNAME}.out")
CONF_EOF
timeout 10s ../tools/rsyslogd -C -n -i"${RSYSLOG_DYNNAME}:strict.pid" -M"${modpath}" \
    -f"${RSYSLOG_DYNNAME}.strict-dirty.conf" >"${RSYSLOG_DYNNAME}.strict-dirty.log" 2>&1
rc=$?
if [ $rc -ne 2 ]; then
    echo "FAIL: expected secure strict mode to abort unclean config with rc 2"
    cat "${RSYSLOG_DYNNAME}.strict-dirty.log"
    error_exit 1
fi
content_check 'global(AbortOnUncleanConfig="on") is set' "${RSYSLOG_DYNNAME}.strict-dirty.log"

exit_test
