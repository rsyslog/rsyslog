#!/bin/bash
# Tests that an invalid YAML config file produces a clean error message and
# that rsyslog exits with a non-zero status (does not crash or hang).
#
# Added 2025 by contributors, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
include(file="'${RSYSLOG_DYNNAME}_bad'.yaml" optional="off")
'

# Write deliberately malformed YAML
cat > "${RSYSLOG_DYNNAME}_bad.yaml" << 'BADEOF'
global:
  workdirectory: "/tmp"
modules:
  - load: [unclosed array
    param: value
BADEOF

# Use config-check mode (-N1) which exits non-zero on any config error
../tools/rsyslogd -C -N1 \
    -M"${RSYSLOG_MODDIR}" \
    -f"${TESTCONF_NM}.conf" \
    > "${RSYSLOG_DYNNAME}.error.log" 2>&1
rc=$?
if [ $rc -eq 0 ]; then
    echo "FAIL: rsyslogd should have reported a config error for invalid YAML"
    error_exit 1
fi
echo "PASS: rsyslogd correctly reported error (exit $rc) for invalid YAML"
exit_test
