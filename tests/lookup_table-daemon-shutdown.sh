#!/bin/bash
# Regression coverage for lookup-table shutdown in daemon mode.
#
# Issues #1071 and #5301 reported crashes or hangs while rsyslog shut down
# with lookup tables configured. This test keeps the setup intentionally small:
# one lookup table, one lookup() expression, daemonized startup, then a normal
# TERM shutdown. The oracle is wait_shutdown returning successfully; a stuck
# lookup reloader thread, recursive lookup rwlock deadlock, or invalid pthread
# teardown causes the testbench shutdown wait to fail.
#
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
skip_TSAN
generate_conf
add_conf '
lookup_table(name="xlate" file="'$RSYSLOG_DYNNAME'.xlate.lkp_tbl" reloadOnHUP="on")

template(name="outfmt" type="string" string="- %msg% %$.lkp%\n")

set $.lkp = lookup("xlate", $msg);

action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")
'
cp -f $srcdir/testsuites/xlate.lkp_tbl $RSYSLOG_DYNNAME.xlate.lkp_tbl
export RSTB_DAEMONIZE="YES"
startup
injectmsg 0 3
wait_queueempty
content_check "msgnum:00000000: foo_old"
content_check "msgnum:00000001: bar_old"
assert_content_missing "baz"
shutdown_when_empty
wait_shutdown
exit_test
