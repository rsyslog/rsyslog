#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# added 2015-09-29 by singh.janmejay
# Check ordinary and out-of-range occurrence indices without short-integer wrap.
# After synchronized shutdown, exact output must retain the ordinary capture and
# return the fallback for absent occurrences, including signed numeric boundaries.
# This file is part of the rsyslog project, released under ASL 2.0
echo ===============================================================================
echo \[rscript_re_extract.sh\]: test re_extract rscript-fn
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
template(name="outfmt" type="string" string="*Number is %$.number%* %$.indices% %$.large%\n")

module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" address="127.0.0.1" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")'
add_conf "
if \$inputname != 'imtcp' then stop
set \$.number = re_extract(\$msg, '.* ([0-9]+)$', 0, 1, 'none');"
add_conf '
# Build 65536 characters by doubling, so the last occurrence proves that the
# search counter and the requested index both retain their full numeric width.
set $.large = "x";
set $.large = $.large & $.large;
set $.large = $.large & $.large;
set $.large = $.large & $.large;
set $.large = $.large & $.large;
set $.large = $.large & $.large;
set $.large = $.large & $.large;
set $.large = $.large & $.large;
set $.large = $.large & $.large;
set $.large = $.large & $.large;
set $.large = $.large & $.large;
set $.large = $.large & $.large;
set $.large = $.large & $.large;
set $.large = $.large & $.large;
set $.large = $.large & $.large;
set $.large = $.large & $.large;
set $.large = $.large & $.large;
set $.large = re_extract($.large & "y", ".", 65536, 0, "none");
set $.indices = re_extract("xy", ".", 0, 0, "none") & "," &
    re_extract("xy", ".", 1, 0, "none") & "," &
    re_extract("xy", ".", 2, 0, "none") & "," &
    re_extract("xy", ".", -1, 0, "none") & "," &
    re_extract("xy", ".", 32767, 0, "none") & "," &
    re_extract("xy", ".", 32768, 0, "none") & "," &
    re_extract("xy", ".", 65535, 0, "none") & "," &
    re_extract("xy", ".", 65536, 0, "none") & "," &
    re_extract("xy", ".", 65537, 0, "none") & "," &
    re_extract("xy", ".", 9223372036854775807, 0, "none");
action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
'
startup
tcpflood -m 1 -I $srcdir/testsuites/date_time_msg
echo doing shutdown
shutdown_when_empty
echo wait on shutdown
wait_shutdown 
export EXPECTED="*Number is 19597* x,y,none,none,none,none,none,none,none,none y"
cmp_exact
exit_test
