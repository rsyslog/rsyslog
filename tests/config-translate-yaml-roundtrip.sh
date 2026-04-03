#!/bin/bash
# check canonical YAML -> RainerScript -> YAML round-trip.
#
# Part of the testbench for rsyslog.
#
# This file is part of rsyslog.
# Released under ASL 2.0
. ${srcdir:=.}/diag.sh init
export ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=0}"
modpath="../runtime/.libs:../plugins/imtcp/.libs:../.libs"
outlog="/tmp/${RSYSLOG_DYNNAME}.roundtrip.log"
portfile="${RSYSLOG_DYNNAME}.tcpflood_port"

cat > "${RSYSLOG_DYNNAME}.yaml" <<YAML_EOF
version: 2

global:
  defaultNetstreamDriverCAFile: "${srcdir}/tls-certs/ca.pem"
  defaultNetstreamDriverCertFile: "${srcdir}/tls-certs/cert.pem"
  defaultNetstreamDriverKeyFile: "${srcdir}/tls-certs/key.pem"

modules:
  - load: "../plugins/imtcp/.libs/imtcp"
    PermittedPeer: ["rsyslog-client"]
    StreamDriver.AuthMode: "x509/name"
    StreamDriver.Mode: "1"
    StreamDriver.Name: "gtls"
inputs:
  - type: "imtcp"
    listenPortFileName: "${portfile}"
    port: "0"
templates:
  - name: "outfmt"
    type: "string"
    string: "%msg:F,58:2%\\n"
rulesets:
  - name: "main"
    actions:
      - type: "omfile"
        file: "${outlog}"
        template: "outfmt"
YAML_EOF

../tools/rsyslogd -N1 -f "${RSYSLOG_DYNNAME}.yaml" -F rainerscript -o "${RSYSLOG_DYNNAME}.roundtrip.conf" -M"$modpath" || error_exit $?
../tools/rsyslogd -N1 -f "${RSYSLOG_DYNNAME}.roundtrip.conf" -F yaml -o "${RSYSLOG_DYNNAME}.roundtrip.yaml" -M"$modpath" || error_exit $?

cat > "${RSYSLOG_DYNNAME}.expected.yaml" <<YAML_EOF
version: 2

global:
  defaultNetstreamDriverCAFile: "${srcdir}/tls-certs/ca.pem"
  defaultNetstreamDriverCertFile: "${srcdir}/tls-certs/cert.pem"
  defaultNetstreamDriverKeyFile: "${srcdir}/tls-certs/key.pem"

modules:
  - load: "../plugins/imtcp/.libs/imtcp"
    PermittedPeer: ["rsyslog-client"]
    StreamDriver.AuthMode: "x509/name"
    StreamDriver.Mode: "1"
    StreamDriver.Name: "gtls"
inputs:
  - type: "imtcp"
    listenPortFileName: "${portfile}"
    port: "0"
templates:
  - name: "outfmt"
    type: "string"
    string: "%msg:F,58:2%\\n"
rulesets:
  - name: "main"
    actions:
      - type: "omfile"
        file: "${outlog}"
        template: "outfmt"
YAML_EOF
cmp_exact_file "${RSYSLOG_DYNNAME}.expected.yaml" "${RSYSLOG_DYNNAME}.roundtrip.yaml"

../tools/rsyslogd -N1 -f "${RSYSLOG_DYNNAME}.roundtrip.yaml" -M"$modpath" || error_exit $?

echo SUCCESS: YAML round-trip translation
exit_test
