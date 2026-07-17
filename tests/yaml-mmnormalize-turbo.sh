#!/bin/bash
# Verify that the YAML front end passes turbo to mmnormalize and exposes the
# normalized CEE field. The output file is the observable success oracle.
. ${srcdir:=.}/diag.sh init
require_plugin imtcp
require_plugin mmnormalize
generate_conf --yaml-only
add_yaml_conf 'modules:'
add_yaml_conf '  - load: "../plugins/imtcp/.libs/imtcp"'
add_yaml_conf '  - load: "../plugins/mmnormalize/.libs/mmnormalize"'
add_yaml_conf 'inputs:'
add_yaml_conf '  - type: imtcp'
add_yaml_conf '    port: "0"'
add_yaml_conf '    listenPortFileName: "'$RSYSLOG_DYNNAME'.tcpflood_port"'
add_yaml_conf '    ruleset: normalize'
add_yaml_conf 'templates:'
add_yaml_conf '  - name: outfmt'
add_yaml_conf '    type: string'
add_yaml_conf '    string: "%$!host% %$!tag%\n"'
add_yaml_conf 'rulesets:'
add_yaml_conf '  - name: normalize'
add_yaml_conf '    statements:'
add_yaml_conf '      - type: mmnormalize'
add_yaml_conf '        useRawMsg: on'
add_yaml_conf '        turbo: on'
add_yaml_conf '        rule: "rule=:%host:word% %tag:char-to:\\x3a%: no longer listening on %ip:ipv4%#%port:number%"'
add_yaml_conf '      - type: omfile'
add_yaml_conf '        file: "'$RSYSLOG_OUT_LOG'"'
add_yaml_conf '        template: outfmt'
startup
tcpflood -m1 -M "\"ubuntu tag1: no longer listening on 127.168.0.1#10514\""
shutdown_when_empty
wait_shutdown
export EXPECTED='ubuntu tag1'
cmp_exact "$RSYSLOG_OUT_LOG"
exit_test
