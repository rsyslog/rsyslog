#!/bin/bash
# addd 2017-01142 by RGerhards, released under ASL 2.0

. $srcdir/diag.sh init
. $srcdir/diag.sh generate-conf
. $srcdir/diag.sh add-conf '
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="13514")
global(parser.PermitSlashInProgramname="on")

template(name="outfmt" type="string" string="%syslogtag%,%programname%\n")
local0.* action(type="omfile" template="outfmt"
	        file="rsyslog.out.log")
'
. $srcdir/diag.sh startup
. $srcdir/diag.sh tcpflood -m 1 -M "\"<133>2011-03-01T11:22:12Z host tag/with/slashes msgh ...x\""
. $srcdir/diag.sh tcpflood -m1
. $srcdir/diag.sh shutdown-when-empty
. $srcdir/diag.sh wait-shutdown
echo "tag/with/slashes,tag/with/slashes" | cmp - rsyslog.out.log
if [ ! $? -eq 0 ]; then
  echo "invalid output generated, rsyslog.out.log is:"
  cat rsyslog.out.log
  . $srcdir/diag.sh error-exit 1
fi;

. $srcdir/diag.sh exit
