#!/bin/bash
echo \[imuxsock_logger_ratelimit.sh\]: test rate limiting with imuxsock
. ${srcdir:=.}/diag.sh init

./syslog_caller -fsyslog_inject-l -m0 > /dev/null 2>&1
no_liblogging_stdlog=$?
if [ $no_liblogging_stdlog -ne 0 ];then
  echo "liblogging-stdlog not available - skipping test"
  exit 77
fi

export EXPECTED=" test message nbr 0, severity=6
 test message nbr 1, severity=6
 test message nbr 2, severity=6
 test message nbr 3, severity=6
 test message nbr 4, severity=6"

for use_special_parser in on off; do
  echo \[imuxsock_logger_ratelimit.sh\]: test rate limiting with imuxsock with useSpecialParser="$use_special_parser"
  echo -n >"$RSYSLOG_OUT_LOG"
  generate_conf
  add_conf '
  module(load="../plugins/imuxsock/.libs/imuxsock" sysSock.use="off")
  input(	type="imuxsock" socket="'$RSYSLOG_DYNNAME'-testbench_socket"
    useSpecialParser="'$use_special_parser'"
    ruleset="testruleset"
    ratelimit.interval="10"
    ratelimit.burst="5")
  template(name="outfmt" type="string" string="%msg:%\n")

  ruleset(name="testruleset") {
    action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
  }
  '
  startup

  ./syslog_caller -m20 -C "uxsock:$RSYSLOG_DYNNAME-testbench_socket" -s6

  # the sleep below is needed to prevent too-early termination of rsyslogd
  ./msleep 100

  shutdown_when_empty
  wait_shutdown

  cmp_exact
done
exit_test
