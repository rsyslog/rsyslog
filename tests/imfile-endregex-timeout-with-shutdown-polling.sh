#!/bin/bash
# This is part of the rsyslog testbench, licensed under ASL 2.0
. $srcdir/diag.sh init
. $srcdir/diag.sh generate-conf
. $srcdir/diag.sh add-conf '
module(load="../plugins/imfile/.libs/imfile" mode="polling" pollingInterval="1")

input(type="imfile"
      File="./rsyslog.input"
      Tag="file:"
      PersistStateInterval="1"
      readTimeout="3"
      startmsg.regex="^[^ ]")
template(name="outfmt" type="list") {
  constant(value="HEADER ")
  property(name="msg" format="json")
  constant(value="\n")
}
if $msg contains "msgnum:" then
 action(
   type="omfile"
   file="rsyslog.out.log"
   template="outfmt"
 )
'
. $srcdir/diag.sh startup

# we need to sleep a bit between writes to give imfile a chance
# to pick up the data (IN MULTIPLE ITERATIONS!)
echo 'msgnum:0
 msgnum:1' > rsyslog.input
./msleep 8000
echo ' msgnum:2
 msgnum:3' >> rsyslog.input

# we now do a stop and restart of rsyslog. This checks that everything
# works across restarts.
. $srcdir/diag.sh shutdown-when-empty # shut down rsyslogd when done processing messages
. $srcdir/diag.sh wait-shutdown    # we need to wait until rsyslogd is finished!

#echo DROPPING YOU TO BASH!
#bash

. $srcdir/diag.sh startup

# new data
echo ' msgnum:4' >> rsyslog.input
./msleep 8000
echo ' msgnum:5
 msgnum:6' >> rsyslog.input
./msleep 8000

# the next line terminates our test. It is NOT written to the output file,
# as imfile waits whether or not there is a follow-up line that it needs
# to combine.
#echo 'END OF TEST' >> rsyslog.input
#./msleep 2000

. $srcdir/diag.sh shutdown-when-empty # shut down rsyslogd when done processing messages
. $srcdir/diag.sh wait-shutdown    # we need to wait until rsyslogd is finished!

printf 'HEADER msgnum:0\\\\n msgnum:1
HEADER  msgnum:2\\\\n msgnum:3\\\\n msgnum:4
HEADER  msgnum:5\\\\n msgnum:6\n' | cmp - rsyslog.out.log
if [ ! $? -eq 0 ]; then
  echo "invalid multiline message generated, rsyslog.out.log is:"
  cat rsyslog.out.log
  . $srcdir/diag.sh error-exit 1
fi;

. $srcdir/diag.sh exit
