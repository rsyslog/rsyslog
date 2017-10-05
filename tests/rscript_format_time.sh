#!/bin/bash
# Added 2017-10-03 by Stephen Workman, released under ASL 2.0

export TZ='US/Eastern'

. $srcdir/diag.sh init
. $srcdir/diag.sh generate-conf
. $srcdir/diag.sh add-conf '
module(load="../plugins/imtcp/.libs/imtcp")
module(load="../plugins/omstdout/.libs/omstdout")
input(type="imtcp" port="13514")

set $!datetime!rfc3164 = format_time(1507165811, "date-rfc3164");
set $!datetime!rfc3339 = format_time(1507165811, "date-rfc3339");

set $!datetime!rfc3164Neg = format_time(-1507165811, "date-rfc3164");
set $!datetime!rfc3339Neg = format_time(-1507165811, "date-rfc3339");

set $!datetime!strftime = format_time(1507165811, "%Y-%m-%d");
set $!datetime!inv1 = format_time(1507165811, "");
set $!datetime!inv2 = format_time(1507165811, "%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y");
set $!datetime!inv3 = format_time(1507165811, "deadbeef");

template(name="outfmt" type="string" string="%!datetime%\n")
local4.* action(type="omfile" file="rsyslog.out.log" template="outfmt")
local4.* :omstdout:;outfmt
'

. $srcdir/diag.sh startup
. $srcdir/diag.sh tcpflood -m1 -y | sed 's|\r||'
. $srcdir/diag.sh shutdown-when-empty
. $srcdir/diag.sh wait-shutdown

EXPECTED='{ "rfc3164": "Oct  4 21:10:11", "rfc3339": "2017-10-04T21:10:11-04:00", "rfc3164Neg": "Mar 29 17:49:49", "rfc3339Neg": "1922-03-29T17:49:49-05:00", "strftime": "2017-10-04", "inv1": "1507165811", "inv2": "1507165811", "inv3": "deadbeef" }'

echo "$EXPECTED" | cmp -b rsyslog.out.log

if [[ $? -ne 0 ]]; then
  printf "Invalid function output detected!\n"
  printf "Expected: $EXPECTED\n"
  printf "Got:      "
  cat rsyslog.out.log
  . $srcdir/diag.sh error-exit 1
fi;

. $srcdir/diag.sh exit
