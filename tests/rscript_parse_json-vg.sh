#!/bin/bash
# Added 2017-12-09 by Rainer Gerhards, released under ASL 2.0
. $srcdir/diag.sh init
. $srcdir/diag.sh generate-conf
. $srcdir/diag.sh add-conf '
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="13514")
template(name="outfmt" type="string" string="%$!%\n")

local4.* {
	set $.ret = parse_json("{ \"c1\":\"data\" }", "\$!parsed");
	action(type="omfile" file="rsyslog.out.log" template="outfmt")
}
'

. $srcdir/diag.sh startup-vg
. $srcdir/diag.sh tcpflood -m1
. $srcdir/diag.sh shutdown-when-empty
. $srcdir/diag.sh wait-shutdown-vg
. $srcdir/diag.sh check-exit-vg

# Our fixed and calculated expected results
EXPECTED='{ "parsed": { "c1": "data" } }'
echo $EXPECTED | cmp - rsyslog.out.log
if [[ $? -ne 0 ]]; then
  printf "Invalid function output detected!\n"
  printf "expected:\n$EXPECTED\n"
  printf "rsyslog.out is:\n"
  cat rsyslog.out.log
  . $srcdir/diag.sh error-exit 1
fi;

. $srcdir/diag.sh exit
