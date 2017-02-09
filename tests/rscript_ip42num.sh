#!/bin/bash
# add 2017-02-09 by Jan Gerhards, released under ASL 2.0
. $srcdir/diag.sh init
. $srcdir/diag.sh generate-conf
. $srcdir/diag.sh add-conf '
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="13514")

set $!ip!v1 = ip42num("0.0.0.0");
set $!ip!v2 = ip42num("0.0.0.1");
set $!ip!v3 = ip42num("0.0.1.0");
set $!ip!v4 = ip42num("0.1.0.0");
set $!ip!v5 = ip42num("1.0.0.0");
set $!ip!v6 = ip42num("0.0.0.135");
set $!ip!v7 = ip42num("1.1.1.1");
set $!ip!v8 = ip42num("225.33.1.10");
set $!ip!v9 = ip42num("172.0.0.1");
set $!ip!v10 = ip42num("255.255.255.255");
set $!ip!e1 = ip42num("a");
set $!ip!e2 = ip42num("");
set $!ip!e3 = ip42num("123.4.6.§");
set $!ip!e4 = ip42num("172.0.0.1.");
set $!ip!e5 = ip42num("172.0.0..1");
set $!ip!e6 = ip42num(".172.0.0.1.");






template(name="outfmt" type="string" string="%!ip%\n")
local4.* action(type="omfile" file="rsyslog.out.log" template="outfmt")



'
. $srcdir/diag.sh startup
. $srcdir/diag.sh tcpflood -m1 -y
. $srcdir/diag.sh shutdown-when-empty
. $srcdir/diag.sh wait-shutdown
. $srcdir/diag.sh seq-check 0 999

. $srcdir/diag.sh exit

