#!/bin/bash
# Added 2017-10-03 by Stephen Workman, released under ASL 2.0
. $srcdir/diag.sh init
. $srcdir/diag.sh generate-conf
. $srcdir/diag.sh add-conf '
module(load="../plugins/imtcp/.libs/imtcp")
module(load="../plugins/omstdout/.libs/omstdout")
input(type="imtcp" port="13514")

set $!datetime!rfc3164 = format_time(1507165811, "rfc3164");
set $!datetime!rfc3339 = format_time(1507165811, "rfc3339");
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

. $srcdir/diag.sh exit
