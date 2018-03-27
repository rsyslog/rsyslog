#!/bin/bash
# This is part of the rsyslog testbench, licensed under ASL 2.0
echo ======================================================================
echo [imfile-endregex-timeout-none.sh]
. $srcdir/diag.sh check-inotify
. $srcdir/diag.sh init
. $srcdir/diag.sh generate-conf
. $srcdir/diag.sh add-conf '
module(load="../plugins/imfile/.libs/imfile")
input(type="imfile"
      File="./rsyslog.input"
      Tag="file:"
      PersistStateInterval="1"
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
./msleep 5000 # wait 5 seconds - this shall cause a timeout
echo ' msgnum:2
 msgnum:3' >> rsyslog.input
# the next line terminates our test. It is NOT written to the output file,
# as imfile waits whether or not there is a follow-up line that it needs
# to combine.
echo 'END OF TEST' >> rsyslog.input
./msleep 200

. $srcdir/diag.sh shutdown-when-empty # shut down rsyslogd when done processing messages
. $srcdir/diag.sh wait-shutdown    # we need to wait until rsyslogd is finished!

printf 'HEADER msgnum:0\\\\n msgnum:1\\\\n msgnum:2\\\\n msgnum:3\n' | cmp - rsyslog.out.log
if [ ! $? -eq 0 ]; then
  echo "invalid multiline message generated, rsyslog.out.log is:"
  cat rsyslog.out.log
  exit 1
fi;

. $srcdir/diag.sh exit
