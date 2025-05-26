#!/bin/bash
. ${srcdir:=.}/diag.sh init
skip_platform "SunOS" "This test does not work on Solaris"

./syslog_caller -fsyslog_inject-l -m0 > /dev/null 2>&1
no_liblogging_stdlog=$?
if [ $no_liblogging_stdlog -ne 0 ];then
  echo "liblogging-stdlog not available - skipping test"
  exit 77
fi

export NUMMESSAGES=1
export QUEUE_EMPTY_CHECK_FUNC=wait_file_lines
generate_conf
add_conf '
module(load="../plugins/imuxsock/.libs/imuxsock"
       SysSock.name="'$RSYSLOG_DYNNAME'-testbench_socket")

template(name="outfmt" type="string" string="%msg:%\n")
local1.*	action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")
'
startup
./syslog_caller -fsyslog_inject-l -m1 -C "uxsock:$RSYSLOG_DYNNAME-testbench_socket"
shutdown_when_empty
wait_shutdown
export EXPECTED=" test"
cmp_exact
