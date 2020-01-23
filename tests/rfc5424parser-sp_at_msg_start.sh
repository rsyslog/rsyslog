#!/bin/bash
# checks that in RFC5424 mode SP at beginning of MSG part is properly handled
# This file is part of the rsyslog project, released  under ASL 2.0
# rgerhards, 2019-05-17
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
template(name="outfmt" type="string" string="%msg%--END\n")

if $syslogtag == "tag" then
	action(type="omfile" template="outfmt" file="'$RSYSLOG_OUT_LOG'")
'
startup
injectmsg literal '<13>1 2019-05-15T11:21:57+03:00 domain.tld tag - - - nosd-nosp'
injectmsg literal '<13>1 2019-05-15T11:21:57+03:00 domain.tld tag - - [abc@123 a="b"] sd-nosp'
injectmsg literal '<13>1 2019-05-15T11:21:57+03:00 domain.tld tag - - -  nosd-sp'
injectmsg literal '<13>1 2019-05-15T11:21:57+03:00 domain.tld tag - - [abc@123 a="b"]  sd-sp'
shutdown_when_empty
wait_shutdown
export EXPECTED='nosd-nosp--END
sd-nosp--END
 nosd-sp--END
 sd-sp--END'
cmp_exact
exit_test
