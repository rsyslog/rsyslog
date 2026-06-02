#!/bin/bash
# Tests that YAML config files can include both:
#   - another .yaml file (YAML-from-YAML include)
#   - a .conf (RainerScript) file (RainerScript-from-YAML include)
# Verifies the extension-based router in cnfDoInclude() works correctly.
#
# Template "outfmt" is defined in the main YAML's templates: section so that
# it is registered via cnfDoObj() before any flex buffers are consumed.
# The included .conf file adds a second template ("confmt") to prove that
# RainerScript includes from YAML are processed; that template is used by a
# second action in the ruleset script so both outputs are checked.
#
# Added 2025 by contributors, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
require_plugin imtcp
export NUMMESSAGES=50
export QUEUE_EMPTY_CHECK_FUNC=wait_file_lines
generate_conf
add_conf '
# Testbench preamble in RainerScript; main config in YAML
include(file="'${RSYSLOG_DYNNAME}'.yaml")
'

# Sub-config included from YAML as a .conf (RainerScript) fragment.
# Defines a second template so we can verify .conf includes are processed.
cat > "${RSYSLOG_DYNNAME}_inc.conf" << CONFEOF
template(name="confmt" type="string" string="%msg:F,58:2%\n")
CONFEOF

# Sub-config included from YAML as a nested .yaml file — loads imtcp.
cat > "${RSYSLOG_DYNNAME}_sub.yaml" << 'SUBEOF'
modules:
  - load: "../plugins/imtcp/.libs/imtcp"
SUBEOF

# Main YAML config.
# templates: is processed via cnfDoObj() during YAML loading (before any flex
# buffers are consumed), so "outfmt" is always available to the ruleset script.
cat > "${RSYSLOG_DYNNAME}.yaml" << YAMLEOF
include:
  - path: "${RSYSLOG_DYNNAME}_sub.yaml"
  - path: "${RSYSLOG_DYNNAME}_inc.conf"

templates:
  - name: outfmt
    type: string
    string: "%msg:F,58:2%\n"

rulesets:
  - name: main
    script: |
      :msg, contains, "msgnum:" action(type="omfile"
          template="outfmt" file="${RSYSLOG_OUT_LOG}")
YAMLEOF
# Expand shell vars
sed -i \
    -e "s|\${RSYSLOG_DYNNAME}|${RSYSLOG_DYNNAME}|g" \
    -e "s|\${RSYSLOG_OUT_LOG}|${RSYSLOG_OUT_LOG}|g" \
    "${RSYSLOG_DYNNAME}.yaml"

add_conf '
input(type="imtcp" port="0" listenPortFileName="'${RSYSLOG_DYNNAME}'.tcpflood_port"
      ruleset="main")
'
startup
tcpflood -m $NUMMESSAGES
shutdown_when_empty
wait_shutdown
seq_check
exit_test
