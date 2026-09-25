#!/bin/bash
# Validate compatibility.* global options from YAML configs.
. ${srcdir:=.}/diag.sh init

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

write_yaml_action_config() {
    local cfg="$1"
    local extra_global="$2"
    cat >"${cfg}" <<YAML_EOF
version: 2
global:
${extra_global}
rulesets:
  - name: main
    actions:
      - type: omfile
        file: "${RSYSLOG_DYNNAME}.out"
YAML_EOF
}

write_yaml_syslogd_config() {
    local cfg="$1"
    local mode="$2"
    cat >"${cfg}" <<YAML_EOF
version: 2
global:
  compatibility.configformat.syslogd: "${mode}"
rulesets:
  - name: main
    filter: "mail.*"
    actions:
      - type: omfile
        file: "${RSYSLOG_DYNNAME}.out"
YAML_EOF
}

write_yaml_property_config() {
    local cfg="$1"
    local mode="$2"
    cat >"${cfg}" <<YAML_EOF
version: 2
global:
  compatibility.configformat.property: "${mode}"
rulesets:
  - name: main
    filter: ':msg, contains, "hello"'
    actions:
      - type: omfile
        file: "${RSYSLOG_DYNNAME}.out"
YAML_EOF
}

write_yaml_global_only() {
    local cfg="$1"
    local mode="$2"
    cat >"${cfg}" <<YAML_EOF
version: 2
global:
  compatibility.configformat.legacy: "${mode}"
YAML_EOF
}

write_yaml_tls_warn_config() {
    local cfg="$1"
    cat >"${cfg}" <<YAML_EOF
version: 2
global:
  compatibility.defaults.secure: "warn"
modules:
  - load: "../plugins/imtcp/.libs/imtcp"
inputs:
  - type: imtcp
    address: "127.0.0.1"
    port: "0"
    listenPortFileName: "${RSYSLOG_DYNNAME}.tlswarn.port"
    streamdriver.mode: 0
    streamdriver.name: "gtls"
    streamdriver.authmode: "x509/name"
rulesets:
  - name: main
    actions:
      - type: omfwd
        target: "127.0.0.1"
YAML_EOF
}

write_yaml_omfwd_global_tls_warn_config() {
    local cfg="$1"
    cat >"${cfg}" <<YAML_EOF
version: 2
global:
  compatibility.defaults.secure: "warn"
  defaultNetstreamDriver: "gtls"
rulesets:
  - name: main
    actions:
      - type: omfwd
        target: "127.0.0.1"
        protocol: "tcp"
YAML_EOF
}

write_yaml_omfwd_strict_explicit_mode0_config() {
    local cfg="$1"
    cat >"${cfg}" <<YAML_EOF
version: 2
global:
  compatibility.defaults.secure: "strict"
rulesets:
  - name: main
    actions:
      - type: omfwd
        target: "127.0.0.1"
        protocol: "tcp"
        streamdriver.name: "gtls"
        streamdriver.mode: 0
YAML_EOF
}

write_yaml_tls_anon_warn_config() {
    local cfg="$1"
    cat >"${cfg}" <<YAML_EOF
version: 2
global:
  compatibility.defaults.secure: "warn"
modules:
  - load: "../plugins/imtcp/.libs/imtcp"
inputs:
  - type: imtcp
    address: "127.0.0.1"
    port: "0"
    listenPortFileName: "${RSYSLOG_DYNNAME}.tlsanon.port"
    streamdriver.mode: 1
    streamdriver.name: "gtls"
    streamdriver.authmode: "anon"
rulesets:
  - name: main
    actions:
      - type: omfwd
        target: "127.0.0.1"
        protocol: "tcp"
        streamdriver.mode: 1
        streamdriver.authmode: "anon"
YAML_EOF
}

write_legacy_wrapper() {
    local cfg="$1"
    local yaml_cfg="$2"
    cat >"${cfg}" <<CONF_EOF
include(file="${yaml_cfg}")
\$MainMsgQueueTimeoutShutdown 10000
action(type="omfile" file="${RSYSLOG_DYNNAME}.out")
CONF_EOF
}

write_yaml_action_config "${RSYSLOG_DYNNAME}.all-valid.yaml" \
'  compatibility.configformat.legacy: "disable"
  compatibility.configformat.syslogd: "warn"
  compatibility.configformat.property: "enable"
  compatibility.defaults.secure: "strict"'
run_expect_success "${RSYSLOG_DYNNAME}.all-valid.yaml" "${RSYSLOG_DYNNAME}.all-valid.log"

write_yaml_action_config "${RSYSLOG_DYNNAME}.secure-default.yaml" \
'  compatibility.configformat.legacy: "enable"'
run_expect_success "${RSYSLOG_DYNNAME}.secure-default.yaml" "${RSYSLOG_DYNNAME}.secure-default.log"
content_check 'backward-compatible insecure default in use' "${RSYSLOG_DYNNAME}.secure-default.log"

write_yaml_action_config "${RSYSLOG_DYNNAME}.secure-warn.yaml" \
'  compatibility.defaults.secure: "warn"'
run_expect_success "${RSYSLOG_DYNNAME}.secure-warn.yaml" "${RSYSLOG_DYNNAME}.secure-warn.log"
content_check 'backward-compatible insecure default in use' "${RSYSLOG_DYNNAME}.secure-warn.log"

write_yaml_action_config "${RSYSLOG_DYNNAME}.bad-legacy.yaml" \
'  compatibility.configformat.legacy: "bogus"'
run_expect_failure "${RSYSLOG_DYNNAME}.bad-legacy.yaml" "${RSYSLOG_DYNNAME}.bad-legacy.log"
content_check 'invalid value' "${RSYSLOG_DYNNAME}.bad-legacy.log"
content_check 'compatibility.configformat.legacy' "${RSYSLOG_DYNNAME}.bad-legacy.log"

write_yaml_action_config "${RSYSLOG_DYNNAME}.bad-syslogd.yaml" \
'  compatibility.configformat.syslogd: "bogus"'
run_expect_failure "${RSYSLOG_DYNNAME}.bad-syslogd.yaml" "${RSYSLOG_DYNNAME}.bad-syslogd.log"
content_check 'compatibility.configformat.syslogd' "${RSYSLOG_DYNNAME}.bad-syslogd.log"

write_yaml_action_config "${RSYSLOG_DYNNAME}.bad-property.yaml" \
'  compatibility.configformat.property: "bogus"'
run_expect_failure "${RSYSLOG_DYNNAME}.bad-property.yaml" "${RSYSLOG_DYNNAME}.bad-property.log"
content_check 'compatibility.configformat.property' "${RSYSLOG_DYNNAME}.bad-property.log"

write_yaml_action_config "${RSYSLOG_DYNNAME}.bad-secure.yaml" \
'  compatibility.defaults.secure: "bogus"'
run_expect_failure "${RSYSLOG_DYNNAME}.bad-secure.yaml" "${RSYSLOG_DYNNAME}.bad-secure.log"
content_check 'compatibility.defaults.secure' "${RSYSLOG_DYNNAME}.bad-secure.log"

write_yaml_action_config "${RSYSLOG_DYNNAME}.strict-dirty.yaml" \
'  compatibility.defaults.secure: "strict"
  typo.parameter: "bad"'
timeout 10s ../tools/rsyslogd -C -n -i"${RSYSLOG_DYNNAME}:strict.pid" -M"${modpath}" \
    -f"${RSYSLOG_DYNNAME}.strict-dirty.yaml" >"${RSYSLOG_DYNNAME}.strict-dirty.log" 2>&1
rc=$?
if [ $rc -ne 2 ]; then
    echo "FAIL: expected secure strict mode to abort unclean config with rc 2"
    cat "${RSYSLOG_DYNNAME}.strict-dirty.log"
    error_exit 1
fi
content_check 'global(AbortOnUncleanConfig="on") is set' "${RSYSLOG_DYNNAME}.strict-dirty.log"

for mode in enable warn disable; do
    write_yaml_global_only "${RSYSLOG_DYNNAME}.legacy-${mode}.yaml" "${mode}"
    write_legacy_wrapper "${RSYSLOG_DYNNAME}.legacy-${mode}.conf" "${RSYSLOG_DYNNAME}.legacy-${mode}.yaml"
done
run_expect_success "${RSYSLOG_DYNNAME}.legacy-enable.conf" "${RSYSLOG_DYNNAME}.legacy-enable.log"
run_expect_success "${RSYSLOG_DYNNAME}.legacy-warn.conf" "${RSYSLOG_DYNNAME}.legacy-warn.log"
content_check 'obsolete $-directive syntax encountered' "${RSYSLOG_DYNNAME}.legacy-warn.log"
run_expect_failure "${RSYSLOG_DYNNAME}.legacy-disable.conf" "${RSYSLOG_DYNNAME}.legacy-disable.log"
content_check 'global(compatibility.configformat.legacy="enable")' "${RSYSLOG_DYNNAME}.legacy-disable.log"

for mode in enable warn disable; do
    write_yaml_syslogd_config "${RSYSLOG_DYNNAME}.syslogd-${mode}.yaml" "${mode}"
done
run_expect_success "${RSYSLOG_DYNNAME}.syslogd-enable.yaml" "${RSYSLOG_DYNNAME}.syslogd-enable.log"
run_expect_success "${RSYSLOG_DYNNAME}.syslogd-warn.yaml" "${RSYSLOG_DYNNAME}.syslogd-warn.log"
content_check 'obsolete classic syslogd filter syntax encountered' "${RSYSLOG_DYNNAME}.syslogd-warn.log"
run_expect_failure "${RSYSLOG_DYNNAME}.syslogd-disable.yaml" "${RSYSLOG_DYNNAME}.syslogd-disable.log"
content_check 'global(compatibility.configformat.syslogd="enable")' "${RSYSLOG_DYNNAME}.syslogd-disable.log"

for mode in enable warn disable; do
    write_yaml_property_config "${RSYSLOG_DYNNAME}.property-${mode}.yaml" "${mode}"
done
run_expect_success "${RSYSLOG_DYNNAME}.property-enable.yaml" "${RSYSLOG_DYNNAME}.property-enable.log"
run_expect_success "${RSYSLOG_DYNNAME}.property-warn.yaml" "${RSYSLOG_DYNNAME}.property-warn.log"
content_check 'obsolete classic property-based filter syntax encountered' "${RSYSLOG_DYNNAME}.property-warn.log"
run_expect_failure "${RSYSLOG_DYNNAME}.property-disable.yaml" "${RSYSLOG_DYNNAME}.property-disable.log"
content_check 'global(compatibility.configformat.property="enable")' "${RSYSLOG_DYNNAME}.property-disable.log"

write_yaml_tls_warn_config "${RSYSLOG_DYNNAME}.tls-warn.yaml"
run_expect_success "${RSYSLOG_DYNNAME}.tls-warn.yaml" "${RSYSLOG_DYNNAME}.tls-warn.log"
content_check 'imtcp has TLS-related settings but streamdriver.mode="0"; mode 0 uses plain TCP so TLS is not active' "${RSYSLOG_DYNNAME}.tls-warn.log"
content_check 'omfwd action uses protocol="udp" (without TLS)' "${RSYSLOG_DYNNAME}.tls-warn.log"

write_yaml_omfwd_global_tls_warn_config "${RSYSLOG_DYNNAME}.omfwd-global-driver-warn.yaml"
run_expect_success "${RSYSLOG_DYNNAME}.omfwd-global-driver-warn.yaml" \
    "${RSYSLOG_DYNNAME}.omfwd-global-driver-warn.log"
content_check 'omfwd has TLS-related settings but streamdriver.mode="0"; mode 0 uses plain TCP so TLS is not active' \
    "${RSYSLOG_DYNNAME}.omfwd-global-driver-warn.log"

write_yaml_omfwd_strict_explicit_mode0_config "${RSYSLOG_DYNNAME}.omfwd-strict-explicit-mode0.yaml"
run_expect_failure "${RSYSLOG_DYNNAME}.omfwd-strict-explicit-mode0.yaml" \
    "${RSYSLOG_DYNNAME}.omfwd-strict-explicit-mode0.log"
content_check 'omfwd: compatibility.defaults.secure="strict" rejects explicit streamdriver.mode="0"' \
    "${RSYSLOG_DYNNAME}.omfwd-strict-explicit-mode0.log"

write_yaml_tls_anon_warn_config "${RSYSLOG_DYNNAME}.tls-anon-warn.yaml"
run_expect_success "${RSYSLOG_DYNNAME}.tls-anon-warn.yaml" "${RSYSLOG_DYNNAME}.tls-anon-warn.log"
content_check 'imtcp uses streamdriver.authmode="anon"; server identity is not authenticated, so MITM is possible' "${RSYSLOG_DYNNAME}.tls-anon-warn.log"
content_check 'omfwd uses streamdriver.authmode="anon"; server identity is not authenticated, so MITM is possible' "${RSYSLOG_DYNNAME}.tls-anon-warn.log"

# YAML parity for the explicit streamdriver.mode="0" cases (bStrmDrvrModeSet).
cat >"${RSYSLOG_DYNNAME}.omfwd-plain-explicit-mode0.yaml" <<YAML_EOF
version: 2
global:
  compatibility.defaults.secure: "warn"
rulesets:
  - name: main
    actions:
      - type: omfwd
        target: "127.0.0.1"
        protocol: "tcp"
        streamdriver.mode: 0
YAML_EOF
run_expect_success "${RSYSLOG_DYNNAME}.omfwd-plain-explicit-mode0.yaml" "${RSYSLOG_DYNNAME}.omfwd-plain-explicit-mode0.log"
check_not_present 'omfwd action uses protocol="tcp" with streamdriver.mode="0"' "${RSYSLOG_DYNNAME}.omfwd-plain-explicit-mode0.log"

cat >"${RSYSLOG_DYNNAME}.imtcp-plain-explicit-mode0.yaml" <<YAML_EOF
version: 2
global:
  compatibility.defaults.secure: "warn"
modules:
  - load: "../plugins/imtcp/.libs/imtcp"
inputs:
  - type: imtcp
    address: "127.0.0.1"
    port: "0"
    listenPortFileName: "${RSYSLOG_DYNNAME}.imtcp-plain-explicit-mode0.port"
    streamdriver.mode: 0
rulesets:
  - name: main
    actions:
      - type: omfile
        file: "${RSYSLOG_DYNNAME}.out"
YAML_EOF
run_expect_success "${RSYSLOG_DYNNAME}.imtcp-plain-explicit-mode0.yaml" "${RSYSLOG_DYNNAME}.imtcp-plain-explicit-mode0.log"
check_not_present 'imtcp input uses streamdriver.mode="0"' "${RSYSLOG_DYNNAME}.imtcp-plain-explicit-mode0.log"

# YAML parity for the explicit protocol="udp" cases: the YAML frontend must set
# bProtocolSet the same way. Explicit UDP is acknowledged (silent), a global
# defaultNetstreamDriver stays irrelevant to UDP (silent), but action-level TLS
# settings still warn.
cat >"${RSYSLOG_DYNNAME}.omfwd-udp-explicit.yaml" <<YAML_EOF
version: 2
global:
  compatibility.defaults.secure: "warn"
rulesets:
  - name: main
    actions:
      - type: omfwd
        target: "127.0.0.1"
        protocol: "udp"
YAML_EOF
run_expect_success "${RSYSLOG_DYNNAME}.omfwd-udp-explicit.yaml" "${RSYSLOG_DYNNAME}.omfwd-udp-explicit.log"
check_not_present 'omfwd action uses protocol="udp"' "${RSYSLOG_DYNNAME}.omfwd-udp-explicit.log"

cat >"${RSYSLOG_DYNNAME}.omfwd-udp-explicit-tls.yaml" <<YAML_EOF
version: 2
global:
  compatibility.defaults.secure: "warn"
rulesets:
  - name: main
    actions:
      - type: omfwd
        target: "127.0.0.1"
        protocol: "udp"
        streamdriver.name: "gtls"
YAML_EOF
run_expect_success "${RSYSLOG_DYNNAME}.omfwd-udp-explicit-tls.yaml" "${RSYSLOG_DYNNAME}.omfwd-udp-explicit-tls.log"
content_check 'omfwd has TLS-related settings but protocol="udp"' "${RSYSLOG_DYNNAME}.omfwd-udp-explicit-tls.log"

cat >"${RSYSLOG_DYNNAME}.omfwd-udp-explicit-cafile.yaml" <<YAML_EOF
version: 2
global:
  compatibility.defaults.secure: "warn"
rulesets:
  - name: main
    actions:
      - type: omfwd
        target: "127.0.0.1"
        protocol: "udp"
        streamdriver.cafile: "${RSYSLOG_DYNNAME}-ca.pem"
YAML_EOF
run_expect_success "${RSYSLOG_DYNNAME}.omfwd-udp-explicit-cafile.yaml" "${RSYSLOG_DYNNAME}.omfwd-udp-explicit-cafile.log"
content_check 'omfwd has TLS-related settings but protocol="udp"' "${RSYSLOG_DYNNAME}.omfwd-udp-explicit-cafile.log"

cat >"${RSYSLOG_DYNNAME}.omfwd-udp-explicit-globaldrvr.yaml" <<YAML_EOF
version: 2
global:
  compatibility.defaults.secure: "warn"
  defaultNetstreamDriver: "gtls"
rulesets:
  - name: main
    actions:
      - type: omfwd
        target: "127.0.0.1"
        protocol: "udp"
YAML_EOF
run_expect_success "${RSYSLOG_DYNNAME}.omfwd-udp-explicit-globaldrvr.yaml" "${RSYSLOG_DYNNAME}.omfwd-udp-explicit-globaldrvr.log"
check_not_present 'protocol="udp"' "${RSYSLOG_DYNNAME}.omfwd-udp-explicit-globaldrvr.log"

# YAML parity for the explicit RELP tls="off" cases (bEnableTLSSet).
if ls ../plugins/omrelp/.libs/omrelp.* >/dev/null 2>&1 && ls ../plugins/imrelp/.libs/imrelp.* >/dev/null 2>&1; then
    cat >"${RSYSLOG_DYNNAME}.omrelp-explicit.yaml" <<YAML_EOF
version: 2
global:
  compatibility.defaults.secure: "warn"
modules:
  - load: "../plugins/omrelp/.libs/omrelp"
rulesets:
  - name: main
    actions:
      - type: omrelp
        target: "127.0.0.1"
        port: "514"
        tls: "off"
YAML_EOF
    run_expect_success "${RSYSLOG_DYNNAME}.omrelp-explicit.yaml" "${RSYSLOG_DYNNAME}.omrelp-explicit.log"
    check_not_present 'omrelp action uses tls="off"' "${RSYSLOG_DYNNAME}.omrelp-explicit.log"

    cat >"${RSYSLOG_DYNNAME}.omrelp-off-with-tls.yaml" <<YAML_EOF
version: 2
global:
  compatibility.defaults.secure: "warn"
modules:
  - load: "../plugins/omrelp/.libs/omrelp"
rulesets:
  - name: main
    actions:
      - type: omrelp
        target: "127.0.0.1"
        port: "514"
        tls: "off"
        tls.authmode: "anon"
YAML_EOF
    run_expect_success "${RSYSLOG_DYNNAME}.omrelp-off-with-tls.yaml" "${RSYSLOG_DYNNAME}.omrelp-off-with-tls.log"
    content_check 'omrelp has TLS-related settings but tls="off"' "${RSYSLOG_DYNNAME}.omrelp-off-with-tls.log"

    cat >"${RSYSLOG_DYNNAME}.omrelp-off-with-zip.yaml" <<YAML_EOF
version: 2
global:
  compatibility.defaults.secure: "warn"
modules:
  - load: "../plugins/omrelp/.libs/omrelp"
rulesets:
  - name: main
    actions:
      - type: omrelp
        target: "127.0.0.1"
        port: "514"
        tls: "off"
        tls.compression: "on"
YAML_EOF
    run_expect_success "${RSYSLOG_DYNNAME}.omrelp-off-with-zip.yaml" "${RSYSLOG_DYNNAME}.omrelp-off-with-zip.log"
    content_check 'omrelp has TLS-related settings but tls="off"' "${RSYSLOG_DYNNAME}.omrelp-off-with-zip.log"

    cat >"${RSYSLOG_DYNNAME}.imrelp-explicit.yaml" <<YAML_EOF
version: 2
global:
  compatibility.defaults.secure: "warn"
modules:
  - load: "../plugins/imrelp/.libs/imrelp"
inputs:
  - type: imrelp
    port: "0"
    tls: "off"
rulesets:
  - name: main
    actions:
      - type: omfile
        file: "${RSYSLOG_DYNNAME}.out"
YAML_EOF
    run_expect_success "${RSYSLOG_DYNNAME}.imrelp-explicit.yaml" "${RSYSLOG_DYNNAME}.imrelp-explicit.log"
    check_not_present 'imrelp input uses tls="off"' "${RSYSLOG_DYNNAME}.imrelp-explicit.log"

    cat >"${RSYSLOG_DYNNAME}.imrelp-off-with-tls.yaml" <<YAML_EOF
version: 2
global:
  compatibility.defaults.secure: "warn"
modules:
  - load: "../plugins/imrelp/.libs/imrelp"
inputs:
  - type: imrelp
    port: "0"
    tls: "off"
    tls.authmode: "anon"
rulesets:
  - name: main
    actions:
      - type: omfile
        file: "${RSYSLOG_DYNNAME}.out"
YAML_EOF
    run_expect_success "${RSYSLOG_DYNNAME}.imrelp-off-with-tls.yaml" "${RSYSLOG_DYNNAME}.imrelp-off-with-tls.log"
    content_check 'imrelp has TLS-related settings but tls="off"' "${RSYSLOG_DYNNAME}.imrelp-off-with-tls.log"

    cat >"${RSYSLOG_DYNNAME}.imrelp-off-with-zip-dh.yaml" <<YAML_EOF
version: 2
global:
  compatibility.defaults.secure: "warn"
modules:
  - load: "../plugins/imrelp/.libs/imrelp"
inputs:
  - type: imrelp
    port: "0"
    tls: "off"
    tls.compression: "on"
    tls.dhbits: "1024"
rulesets:
  - name: main
    actions:
      - type: omfile
        file: "${RSYSLOG_DYNNAME}.out"
YAML_EOF
    run_expect_success "${RSYSLOG_DYNNAME}.imrelp-off-with-zip-dh.yaml" "${RSYSLOG_DYNNAME}.imrelp-off-with-zip-dh.log"
    content_check 'imrelp has TLS-related settings but tls="off"' "${RSYSLOG_DYNNAME}.imrelp-off-with-zip-dh.log"
fi

exit_test
