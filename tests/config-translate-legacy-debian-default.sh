#!/bin/bash
# check translation of Debian-style legacy default rules into YAML statements.
#
# Part of the testbench for rsyslog.
#
# This file is part of rsyslog.
# Released under ASL 2.0
. ${srcdir:=.}/diag.sh init
export ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=0}"
modpath="../runtime/.libs:../.libs"

cat > "${RSYSLOG_DYNNAME}.conf" <<'RS_EOF'
*.*;auth,authpriv.none		-/var/log/syslog
auth,authpriv.*			/var/log/auth.log
cron.*				-/var/log/cron.log
kern.*				-/var/log/kern.log
mail.*				-/var/log/mail.log
user.*				-/var/log/user.log
*.emerg				:omusrmsg:*
RS_EOF

../tools/rsyslogd -N1 -f "${RSYSLOG_DYNNAME}.conf" -F yaml -o "${RSYSLOG_DYNNAME}.yaml" -M"$modpath" || error_exit $?
cat -n "${RSYSLOG_DYNNAME}.yaml"

cat > "${RSYSLOG_DYNNAME}.expected.yaml" <<'YAML_EOF'
version: 2

rulesets:
  # TRANSLATION WARNING: top-level statements normalized into explicit RSYSLOG_DefaultRuleset
  - name: "RSYSLOG_DefaultRuleset"
    statements:
      - if: "prifilt('*.*;auth,authpriv.none')"
        action:
          type: "omfile"
          file: "/var/log/syslog"
      - if: "prifilt('auth,authpriv.*')"
        action:
          type: "omfile"
          file: "/var/log/auth.log"
      - if: "prifilt('cron.*')"
        action:
          type: "omfile"
          file: "/var/log/cron.log"
      - if: "prifilt('kern.*')"
        action:
          type: "omfile"
          file: "/var/log/kern.log"
      - if: "prifilt('mail.*')"
        action:
          type: "omfile"
          file: "/var/log/mail.log"
      - if: "prifilt('user.*')"
        action:
          type: "omfile"
          file: "/var/log/user.log"
      - if: "prifilt('*.emerg')"
        action:
          type: "omusrmsg"
          users: "*"
YAML_EOF
cmp_exact_file "${RSYSLOG_DYNNAME}.expected.yaml" "${RSYSLOG_DYNNAME}.yaml"

echo SUCCESS: Debian-style legacy defaults translate into YAML statements
exit_test
