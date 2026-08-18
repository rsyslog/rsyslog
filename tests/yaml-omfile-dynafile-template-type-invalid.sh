#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
#
# Copyright 2026 Rainer Gerhards and Adiscon GmbH.
#
# Ensure a YAML omfile module default rejects an uninspectable dynafile
# template on a modern YAML action. Configuration validation must fail with the
# strict-template diagnostic, proving both YAML parameter delivery and action
# propagation.
. ${srcdir:=.}/diag.sh init

generate_conf
add_conf '
include(file="'${RSYSLOG_DYNNAME}'.yaml")
'

cat > "${RSYSLOG_DYNNAME}.yaml" << 'YAMLEOF'
modules:
  - load: "builtin:omfile"
    dynafile.restrictTemplateType: "on"

templates:
  - name: dynfile
    type: subtree
    subtree: "$!dynfile"
  - name: outfmt
    type: string
    string: "%msg:F,58:2%\\n"

rulesets:
  - name: main
    actions:
      - type: omfile
        dynafile: dynfile
        template: outfmt
YAMLEOF

if ../tools/rsyslogd -C -N1 -M"$RSYSLOG_MODDIR" -f"${TESTCONF_NM}.conf" \
	>"${RSYSLOG_DYNNAME}.log" 2>&1; then
	echo "FAIL: expected YAML config validation failure for subtree dynafile template"
	cat "${RSYSLOG_DYNNAME}.log"
	error_exit 1
else
	exit_code=$?
	if [ "$exit_code" -ne 1 ]; then
		echo "FAIL: expected exit code 1, got $exit_code"
		cat "${RSYSLOG_DYNNAME}.log"
		error_exit 1
	fi
fi

grep -F "dynafile template 'dynfile' uses a template type that cannot be safely inspected" \
	"${RSYSLOG_DYNNAME}.log" >/dev/null || {
	echo "FAIL: expected dynafile template type validation error"
	cat "${RSYSLOG_DYNNAME}.log"
	error_exit 1
}

exit_test
