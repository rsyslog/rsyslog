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
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")

module(load="../contrib/fmpansanitizer/.libs/fmpansanitizer")
set $.mask_1 = maskpan($msg,"(4\\d{5}|5[12345]\\d{4})\\d{6}(\\d{4})","\$1******\$2");

action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")
'
startup
injectmsg_literal "foo bar ABCD4000000000000002____4000000000000001__604000000000000002__"
echo doing shutdown
shutdown_when_empty
echo wait on shutdown
wait_shutdown
# The first and third PAN are valid and masked, the second is invalid (fails Luhn check) and not masked.
. $srcdir/diag.sh content-pattern-check "ABCD400000\*\*\*\*\*\*0002____4000000000000001__60400000\*\*\*\*\*\*0002__"
exit_test
