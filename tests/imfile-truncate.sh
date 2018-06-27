#!/bin/bash
# addd 2016-10-06 by RGerhards, released under ASL 2.0
echo [imfile-truncate.sh]
. $srcdir/diag.sh check-inotify
. $srcdir/diag.sh init
export RSYSLOG_DEBUG="debug nologfuncflow noprintmutexaction stdout"
. $srcdir/diag.sh generate-conf
. $srcdir/diag.sh add-conf '
module(load="../plugins/imfile/.libs/imfile")

input(type="imfile"
      File="./rsyslog.input"
      Tag="file:"
      reopenOnTruncate="on"
     )

template(name="outfmt" type="string" string="%msg:F,58:2%\n")

if $msg contains "msgnum:" then
 action(
   type="omfile"
   file="rsyslog.out.log"
   template="outfmt"
 )
'
. $srcdir/diag.sh startup

# write the beginning of the file
echo 'msgnum:0
msgnum:1' > rsyslog.input

set -x
# sleep a little to give rsyslog a chance to begin processing
sleep 1

# truncate and write some more lines (see https://github.com/rsyslog/rsyslog/issues/1090)
echo 'msgnum:2' > rsyslog.input
# sleep some more
sleep 1
echo 'msgnum:3
msgnum:4' >> rsyslog.input

# give it time to finish
sleep 1
set -x

. $srcdir/diag.sh shutdown-when-empty # shut down rsyslogd when done processing messages
. $srcdir/diag.sh wait-shutdown

. $srcdir/diag.sh seq-check 0 4
. $srcdir/diag.sh exit
