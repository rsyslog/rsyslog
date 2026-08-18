#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
#
# Copyright 2026 Rainer Gerhards and Adiscon GmbH.
#
# Check dynafile traversal with a YAML-defined list template.
. ${srcdir:=.}/diag.sh init
require_plugin imtcp

export TRAVERSAL_APP="${RSYSLOG_DYNNAME}-yaml-omfile-list-template-traversal"
export TRAVERSAL_ROOT="${RSYSLOG_DYNNAME}.jail"
export TRAVERSAL_OUT="${TRAVERSAL_ROOT}/escape/${TRAVERSAL_APP}.log"
export PERMIT_OUT="${TRAVERSAL_ROOT}/escape/${TRAVERSAL_APP}.permit.log"
rm -rf "$TRAVERSAL_ROOT"
mkdir -p "${TRAVERSAL_ROOT}/base" "${TRAVERSAL_ROOT}/base-permit" "${TRAVERSAL_ROOT}/escape"

generate_conf
add_conf '
include(file="'${RSYSLOG_DYNNAME}'.yaml")
'

cat > "${RSYSLOG_DYNNAME}.yaml" << YAMLEOF
modules:
  - load: "../plugins/imtcp/.libs/imtcp"

templates:
  - name: dynfile
    type: list
    elements:
      - constant:
          value: "${TRAVERSAL_ROOT}/base/"
      - property:
          name: hostname
      - constant:
          value: "/"
      - property:
          name: app-name
      - constant:
          value: ".log"
  - name: outfmt
    type: string
    string: "%msg%\n"
  - name: dynfile_permit
    type: list
    elements:
      - constant:
          value: "${TRAVERSAL_ROOT}/base-permit/"
      - property:
          name: hostname
      - constant:
          value: "/"
      - property:
          name: app-name
      - constant:
          value: ".permit.log"

rulesets:
  - name: main
    filter: 'local4.debug'
    actions:
      - type: omfile
        dynafile: dynfile
        template: outfmt
      - type: omfile
        dynafile: dynfile_permit
        template: outfmt
        dynafile.dangerousPermitPathEscape: "on"
YAMLEOF

add_conf '
input(type="imtcp" port="0" listenPortFileName="'${RSYSLOG_DYNNAME}'.tcpflood_port"
      ruleset="main")
'

startup
printf '<167>1 2003-03-01T01:00:00.000Z ../escape %s - - - traversal-yaml-list-template\n' \
	"$TRAVERSAL_APP" > "${RSYSLOG_DYNNAME}.input"
tcpflood -B -I "${RSYSLOG_DYNNAME}.input"
shutdown_when_empty
wait_shutdown

if [ -f "$TRAVERSAL_OUT" ]; then
	echo "FAIL: YAML list dynafile traversal created output outside template base: $TRAVERSAL_OUT"
	rm -rf "$TRAVERSAL_ROOT"
	error_exit 1
fi

if [ ! -f "$PERMIT_OUT" ]; then
	echo "FAIL: YAML action-level dangerous dynafile path escape did not create $PERMIT_OUT"
	rm -rf "$TRAVERSAL_ROOT"
	error_exit 1
fi

rm -rf "$TRAVERSAL_ROOT"
exit_test
