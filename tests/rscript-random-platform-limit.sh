#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Verify the documented platform cap with a deterministic /dev/urandom read.
# A test-only preload supplies LONG_MIN; the helper derives expected unsigned
# magnitude modulo RAND_MAX using LONG_MAX+1, independently of parser code.
# Exact output after synchronized shutdown proves the capped result and ordinary
# positive/negative/zero controls. The marker proves the shim actually ran.
. ${srcdir:=.}/diag.sh init
[ "$(uname -s)" = Linux ] || { echo 'Linux /proc urandom fixture required'; exit 77; }
[ -d /proc/self/fd ] || { echo 'procfs descriptor lookup required'; exit 77; }
skip_ASAN 'LD_PRELOAD conflicts with ASan runtime load order'
case "${USE_VALGRIND:-}" in
 YES|YES-NOLEAK) echo 'urandom preload is not supported on the Valgrind launcher'; exit 77 ;;
esac
if [ "${RANDOM_LIMIT_YAML:-0}" = 1 ]; then
 require_yaml_support
 require_plugin imtcp
fi
values=$(./random-range-values) || exit $?
read -r maximum expected ordinary <<< "$values"
export RSYSLOG_RANDOM_RANGE_MARKER="$PWD/${RSYSLOG_DYNNAME}.injected"
export RSYSLOG_PRELOAD=".libs/librandom_range_read_preload.so"
if [ "${RANDOM_LIMIT_YAML:-0}" = 1 ]; then
 generate_conf --yaml-only
 add_yaml_conf "
modules:
  - load: ../plugins/imtcp/.libs/imtcp
inputs:
  - type: imtcp
    address: 127.0.0.1
    port: \"0\"
    listenPortFileName: \"${RSYSLOG_DYNNAME}.tcpflood_port\"
    ruleset: main
rulesets:
  - name: main
    script: |
      if \$msg contains \"msgnum:\" then {
      set \$.large = random($maximum);
      set \$.negative = random(-$maximum);
      set \$.ordinary = random(10);
      set \$.negordinary = random(-10);
      set \$.zero = random(0);
      action(type=\"omfile\" file=\"$RSYSLOG_OUT_LOG\" template=\"outfmt\")
      }
templates:
  - name: outfmt
    type: string
    string: \"%\$.large% %\$.negative% %\$.ordinary% %\$.negordinary% %\$.zero%\\n\"
"
else
 generate_conf
 add_conf '
template(name="outfmt" type="string" string="%$.large% %$.negative% %$.ordinary% %$.negordinary% %$.zero%\n")
if $msg contains "msgnum:" then {
set $.large = random('$maximum');
set $.negative = random(-'$maximum');
set $.ordinary = random(10);
set $.negordinary = random(-10);
set $.zero = random(0);
action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
}
'
fi
startup
if [ "${RANDOM_LIMIT_YAML:-0}" = 1 ]; then tcpflood -m1; else injectmsg 0 1; fi
shutdown_when_empty
wait_shutdown
[ -s "$RSYSLOG_RANDOM_RANGE_MARKER" ] || error_exit 1
printf '%s %s %s %s 0\n' "$expected" "$expected" "$ordinary" "$ordinary" > "${RSYSLOG_DYNNAME}.expected"
cmp_exact_file "${RSYSLOG_DYNNAME}.expected" "$RSYSLOG_OUT_LOG"
exit_test
