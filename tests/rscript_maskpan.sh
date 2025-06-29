#!/bin/bash
# added 2025-06-26 by Arnaud Grandville
# This file is part of the rsyslog project, released under ASL 2.0
echo ===============================================================================
echo \rrscript_maskpan.sh\]: test for maskpan script-function
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
template(name="outfmt" type="string" string="%$.mask_1%\n")

module(load="../plugins/imtcp/.libs/imtcp")
module(load="../contrib/fmpansanitizer/.libs/fmpansanitizer")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")

set $.mask_1 = maskpan("ABCD5500000000000327____4000000000000001__605500000000000327__","(4\\d{5}|5[12345]\\d{4})\\d{6}(\\d{4})","\$1******\$2");

action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")
'
startup
tcpflood -m 2
echo doing shutdown
shutdown_when_empty
echo wait on shutdown
wait_shutdown
# . $srcdir/diag.sh content-pattern-check "____550000******0327____4000000000000001____550000******0327__"
exit_test
