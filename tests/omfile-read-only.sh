#!/bin/bash
# addd 2016-06-16 by RGerhards, released under ASL 2.0

messages=20000 # how many messages to inject?
# Note: we need to inject a somewhat larger nubmer of messages in order
# to ensure that we receive some messages in the actual output file,
# as batching can (validly) cause a larger loss in the non-writable
# file

. $srcdir/diag.sh init
. $srcdir/diag.sh generate-conf
. $srcdir/diag.sh add-conf '
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="13514")

template(name="outfmt" type="string" string="%msg:F,58:2%\n")
:msg, contains, "msgnum:" {
	action(type="omfile" template="outfmt" file="rsyslog2.out.log")
	action(type="omfile" template="outfmt" file="rsyslog.out.log"
	       action.execOnlyWhenPreviousIsSuspended="on"
	      )
}
'
touch rsyslog2.out.log
chmod 0400 rsyslog2.out.log
ls -l rsyslog.ou*
. $srcdir/diag.sh startup
$srcdir/diag.sh injectmsg 0 $messages
. $srcdir/diag.sh shutdown-when-empty
. $srcdir/diag.sh wait-shutdown
# we know that the output file is missing some messages, but it
# MUST have some more, and these be in sequence. So we now read
# the first message number and calculate based on it what must be
# present in the output file.
. $srcdir/diag.sh presort
let firstnum=$((10#`$RS_HEADCMD -n1 ${RS_PWORK}work`)) # work is the sorted output file
echo "info: first message expected to be number $firstnum, using that value."
. $srcdir/diag.sh seq-check $firstnum $(($messages-1))
. $srcdir/diag.sh exit
