#!/bin/bash
# added 2015-06-22 by singh.janmejay
# Verifies that random(max) emits bounded non-negative values. The oracle is
# the omfile output after synchronized shutdown, so the check covers the value
# that rsyslog delivered to the configured action destination.
# This file is part of the rsyslog project, released under ASL 2.0
echo ===============================================================================
echo \[rscript_random.sh\]: test for random-number-generator script-function
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
template(name="outfmt" type="string" string="%$.random_no% %$.random_neg%\n")

module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")

set $.random_no = random(10);
set $.random_neg = random(-10);

action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")
'
startup
tcpflood -m 200
echo doing shutdown
shutdown_when_empty
echo wait on shutdown
wait_shutdown 
. $srcdir/diag.sh content-pattern-check "^[0-9] [0-9]$"
exit_test
