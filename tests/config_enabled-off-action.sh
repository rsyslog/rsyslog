#!/bin/bash
# check that an action disabled via config.enabled="off" is not instantiated at
# all - in both the RainerScript and YAML frontends (YAML parsing produces the
# same nvlst and calls the same cnfstmtNewAct() path). Its parameters must not be
# processed, so no "parameter not known" message is emitted for them (an enabled
# action would report it - see config_enabled-on.sh). This also guarantees module
# side effects such as omelasticsearch probing its server in checkCnf never run
# for a disabled action. The oracle is rsyslogd config-check output.
# added 2026-07-18, released under ASL 2.0
. ${srcdir:=.}/diag.sh init

# --- RainerScript frontend ---
generate_conf
add_conf '
action(type="omfile" file="'$RSYSLOG_OUT_LOG'" invalid.param="error" config.enabled="off")
action(type="omfile" file="'$RSYSLOG_OUT_LOG'")
'
export RS_REDIR=">${RSYSLOG_DYNNAME}.rsyslog.log 2>&1"
rsyslogd_config_check
unset RS_REDIR
check_not_present 'parameter.*invalid.param.*not known' "${RSYSLOG_DYNNAME}.rsyslog.log"

# --- YAML frontend (same cnfstmtNewAct() path) ---
cat >"${RSYSLOG_DYNNAME}.disabled.yaml" <<YAML_EOF
version: 2
rulesets:
  - name: main
    actions:
      - type: omfile
        file: "${RSYSLOG_OUT_LOG}"
        invalid.param: "error"
        config.enabled: "off"
      - type: omfile
        file: "${RSYSLOG_OUT_LOG}"
YAML_EOF
../tools/rsyslogd -C -N1 -M"${RSYSLOG_MODDIR}" -f"${RSYSLOG_DYNNAME}.disabled.yaml" \
    >"${RSYSLOG_DYNNAME}.yaml.log" 2>&1
check_not_present 'parameter.*invalid.param.*not known' "${RSYSLOG_DYNNAME}.yaml.log"
exit_test
