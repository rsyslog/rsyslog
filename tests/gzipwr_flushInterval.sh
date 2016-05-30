#!/bin/bash
# This file is part of the rsyslog project, released  under ASL 2.0
. $srcdir/diag.sh init
. $srcdir/diag.sh generate-conf
. $srcdir/diag.sh add-conf '
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="13514")

template(name="outfmt" type="string"
	 string="%msg:F,58:2%\n")
:msg, contains, "msgnum:" action(type="omfile" template="outfmt"
				 zipLevel="6" ioBufferSize="256k"
				 flushOnTXEnd="off" flushInterval="1"
				 asyncWriting="on"
			         file="rsyslog.out.log")
'
. $srcdir/diag.sh startup
. $srcdir/diag.sh tcpflood -m2500 -P129
./msleep 2500
. $srcdir/diag.sh gzip-seq-check 0 2499
. $srcdir/diag.sh tcpflood -i2500 -m2500 -P129
. $srcdir/diag.sh shutdown-when-empty
. $srcdir/diag.sh wait-shutdown
. $srcdir/diag.sh gzip-seq-check 0 4999
. $srcdir/diag.sh exit
