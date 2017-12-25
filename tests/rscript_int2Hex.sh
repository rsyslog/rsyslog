#!/bin/bash
# add 2017-02-09 by Jan Gerhards, released under ASL 2.0
. $srcdir/diag.sh init
. $srcdir/diag.sh generate-conf
. $srcdir/diag.sh add-conf '
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="13514")

set $!ip!v0 = int2hex("");
set $!ip!v1 = int2hex("0");
set $!ip!v2 = int2hex("1");
set $!ip!v4 = int2hex("375894");
set $!ip!v6 = int2hex("16");
set $!ip!v8 = int2hex("4294967295");

set $!ip!e1 = int2hex("a");

template(name="outfmt" type="string" string="%!ip%\n")
local4.* action(type="omfile" file="rsyslog.out.log" template="outfmt")
'
. $srcdir/diag.sh startup
. $srcdir/diag.sh tcpflood -m1 -y
. $srcdir/diag.sh shutdown-when-empty
. $srcdir/diag.sh wait-shutdown
echo '{ "v0": "0", "v1": "0", "v2": "1", "v4": "5bc56", "v6": "10", "v8": "ffffffff", "e1": "NAN" }' | cmp - rsyslog.out.log
if [ ! $? -eq 0 ]; then
  echo "invalid function output detected, rsyslog.out.log is:"
  cat rsyslog.out.log
  . $srcdir/diag.sh error-exit 1
fi;
. $srcdir/diag.sh exit
