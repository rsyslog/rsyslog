#!/bin/bash
# This is part of the rsyslog testbench, licensed under ASL 2.0
. ${srcdir:=.}/diag.sh init
. $srcdir/diag.sh check-inotify-only
export IMFILECHECKTIMEOUT="60"

mkdir "$RSYSLOG_DYNNAME.work"
generate_conf
add_conf '
global(workDirectory="./'"$RSYSLOG_DYNNAME"'.work")
module(load="../plugins/imfile/.libs/imfile" timeoutGranularity="1")

input(type="imfile" File="./'$RSYSLOG_DYNNAME'.input" Tag="file:"
      PersistStateInterval="1" readTimeout="2" startmsg.regex="^[^ ]")

template(name="outfmt" type="list") {
  constant(value="HEADER ")
  property(name="msg" format="json")
  constant(value="\n")
}

if $msg contains "msgnum:" then
	action( type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")
'
startup

# we need to sleep a bit between writes to give imfile a chance
# to pick up the data (IN MULTIPLE ITERATIONS!)
echo 'msgnum:0
 msgnum:1' > $RSYSLOG_DYNNAME.input
content_check_with_count "msgnum:0
 msgnum:1" 1 $IMFILECHECKTIMEOUT

echo ' msgnum:2
 msgnum:3' >> $RSYSLOG_DYNNAME.input
content_check_with_count "msgnum:2
 msgnum:3" 1 $IMFILECHECKTIMEOUT

# the next line terminates our test. It is NOT written to the output file,
# as imfile waits whether or not there is a follow-up line that it needs
# to combine.
echo 'END OF TEST' >> $RSYSLOG_DYNNAME.input

shutdown_when_empty
wait_shutdown
exit_test
