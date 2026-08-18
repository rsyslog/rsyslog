#!/bin/bash
# Validate YAML frontend parity for omfile followSymlinks. The config uses
# backward-compatible global defaults but sets followSymlinks false on the
# action, so the symlink target must remain empty after synchronized shutdown.
. ${srcdir:=.}/diag.sh init
require_plugin imtcp

link="${RSYSLOG_DYNNAME}.yaml.link"
target="${RSYSLOG_DYNNAME}.yaml.target"
normal="${RSYSLOG_DYNNAME}.yaml.normal"

rm -f "$link" "$target" "$normal"
: >"$target"
ln -s "$target" "$link" || skip_test

cat >"${RSYSLOG_DYNNAME}.omfile-follow.yaml" <<YAML_EOF
version: 2
templates:
  - name: outfmt
    type: string
    string: "%msg:F,58:2%\\n"
rulesets:
  - name: main
    statements:
      - if: '\$msg contains "msgnum:"'
        then:
          - type: omfile
            file: "${link}"
            template: outfmt
            followSymlinks: "off"
          - type: omfile
            file: "${normal}"
            template: outfmt
YAML_EOF

generate_conf
add_conf '
global(compatibility.defaults.secure="backward-compatible")
include(file="'${RSYSLOG_DYNNAME}'.omfile-follow.yaml")
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="0" listenPortFileName="'${RSYSLOG_DYNNAME}'.tcpflood_port"
      ruleset="main")
'

startup
tcpflood -m1
shutdown_when_empty
wait_shutdown

if [ -s "$target" ]; then
	echo "FAIL: YAML followSymlinks=false wrote through symlink target"
	cat "$target"
	error_exit 1
fi
content_check "00000000" "$normal"

exit_test
