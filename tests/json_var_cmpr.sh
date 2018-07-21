#!/bin/bash
# added 2015-11-24 by portant
# This file is part of the rsyslog project, released under ASL 2.0
echo =============================================================================================
echo \[json_var_case.sh\]: test for referencing local and global variables properly in comparisons
. $srcdir/diag.sh init
startup json_var_cmpr.conf
. $srcdir/diag.sh tcpflood -m 1 -M "\"<167>Nov  6 12:34:56 172.0.0.1 test: @cee: { \\\"val\\\": \\\"abc\\\" }\""
echo doing shutdown
shutdown_when_empty
echo wait on shutdown
wait_shutdown
. $srcdir/diag.sh content-check  "json prop:abc  local prop:def  global prop:ghi"
exit_test
