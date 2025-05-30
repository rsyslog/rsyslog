#!/bin/bash
# This is part of the rsyslog testbench, licensed under ASL 2.0
echo ======================================================================
echo [imfile-endregex-save-lf-persist.sh]
. ${srcdir:=.}/diag.sh init
. $srcdir/diag.sh check-inotify
mkdir "$RSYSLOG_DYNNAME.work"
generate_conf
add_conf '
global(workDirectory="./'"$RSYSLOG_DYNNAME"'.work")
module(load="../plugins/imfile/.libs/imfile")
input(type="imfile"
      File="./'$RSYSLOG_DYNNAME'.input"
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
   file=`echo $RSYSLOG_OUT_LOG`
   template="outfmt"
 )
'
startup

# we need to sleep a bit between writes to give imfile a chance
# to pick up the data (IN MULTIPLE ITERATIONS!)
echo 'msgnum:0
 msgnum:1
 msgnum:2' > $RSYSLOG_DYNNAME.input
./msleep 300
echo 'msgnum:3' >> $RSYSLOG_DYNNAME.input
echo 'msgnum:4
 msgnum:5' >> $RSYSLOG_DYNNAME.input
./msleep 200
# the next line terminates our test. It is NOT written to the output file,
# as imfile waits whether or not there is a follow-up line that it needs
# to combine.
echo 'END OF TEST' >> $RSYSLOG_DYNNAME.input
./msleep 200

shutdown_when_empty # shut down rsyslogd when done processing messages
wait_shutdown    # we need to wait until rsyslogd is finished!

printf 'HEADER msgnum:0\\\\n msgnum:1\\\\n msgnum:2
HEADER msgnum:3
HEADER msgnum:4\\\\n msgnum:5\n' | cmp - $RSYSLOG_OUT_LOG
if [ ! $? -eq 0 ]; then
  echo "invalid multiline message generated, $RSYSLOG_OUT_LOG is:"
  cat $RSYSLOG_OUT_LOG
  exit 1
fi;

exit_test
