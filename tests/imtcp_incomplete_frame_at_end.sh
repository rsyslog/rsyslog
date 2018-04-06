#!/bin/bash
# Copyright (C) 2016 by Rainer Gerhardds
# This file is part of the rsyslog project, released  under ASL 2.0

. $srcdir/diag.sh init
. $srcdir/diag.sh generate-conf
. $srcdir/diag.sh add-conf '
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="13514")

template(name="outfmt" type="list") {
	property(name="msg")
	constant(value="\n")
}
:msg, contains, "lastmsg" action(type="omfile" template="outfmt"
			         file="rsyslog.out.log")
'
. $srcdir/diag.sh startup
echo -n "<165>1 2003-08-24T05:14:15.000003-07:00 192.0.2.1 tcpflood 8710 - - lastmsg" >tmp.in
. $srcdir/diag.sh tcpflood -I tmp.in
rm tmp.in
./msleep 500
. $srcdir/diag.sh shutdown-when-empty # shut down rsyslogd when done processing messages
. $srcdir/diag.sh wait-shutdown       # and wait for it to terminate
echo "lastmsg" | cmp - rsyslog.out.log
if [ ! $? -eq 0 ]; then
  echo "lastmsg was not properly recorded, file content:"
  cat rsyslog.out.log
  exit 1
fi;
. $srcdir/diag.sh exit
