#!/bin/bash
# Verify that the YAML frontend accepts omfwd PKCS#11 TLS module defaults plus
# an action-level StreamDriver.CAExtraFiles override for the ossl driver. The
# oracle is successful -N1 config validation, which proves the YAML parser and
# shared omfwd config backend accept the new module/action parameter family
# without requiring PKCS#11 runtime setup during the test.
. ${srcdir:=.}/diag.sh init

generate_conf
add_conf '
include(file="'${RSYSLOG_DYNNAME}'.yaml")
'

cat > "${RSYSLOG_DYNNAME}.yaml" << 'YAMLEOF'
modules:
  - load: "builtin:omfwd"
    streamdriver.cafile: "pkcs11:token=rsyslog;object=ca-cert;type=cert"
    streamdriver.caextrafiles: "pkcs11:token=rsyslog;object=intermediate-ca-cert;type=cert"
    streamdriver.certfile: "pkcs11:token=rsyslog;object=client-cert;type=cert"
    streamdriver.keyfile: "pkcs11:token=rsyslog;object=client-key;type=private"

rulesets:
  - name: main
    actions:
      - type: omfwd
        target: "127.0.0.1"
        port: "6514"
        protocol: "tcp"
        streamdriver.name: "ossl"
        streamdriver.mode: 1
        streamdriver.authmode: "x509/certvalid"
        streamdriver.caextrafiles: "pkcs11:token=rsyslog;object=override-intermediate-ca-cert;type=cert"
YAMLEOF

../tools/rsyslogd -N1 -f"${TESTCONF_NM}.conf" -M"$RSYSLOG_MODDIR" >"${RSYSLOG_DYNNAME}.log" 2>&1
if [ $? -ne 0 ]; then
    echo "FAIL: expected YAML config validation to accept omfwd PKCS#11 module defaults"
    cat "${RSYSLOG_DYNNAME}.log"
    error_exit 1
fi

exit_test
