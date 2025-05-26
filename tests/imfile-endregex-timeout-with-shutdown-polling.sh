#!/bin/bash
# This is part of the rsyslog testbench, licensed under ASL 2.0
. ${srcdir:=.}/diag.sh init
export IMFILECHECKTIMEOUT="60"

mkdir "$RSYSLOG_DYNNAME.work"
generate_conf
add_conf '
global(workDirectory="./'"$RSYSLOG_DYNNAME"'.work")
module(load="../plugins/imfile/.libs/imfile" mode="polling" pollingInterval="1")

input(type="imfile"
      File="./'$RSYSLOG_DYNNAME'.input"
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
   file=`echo $RSYSLOG_OUT_LOG`
   template="outfmt"
 )
'
startup

# we need to sleep a bit between writes to give imfile a chance
# to pick up the data (IN MULTIPLE ITERATIONS!)
echo 'msgnum:0
 msgnum:1' > $RSYSLOG_DYNNAME.input	 
content_check_with_count 'msgnum:0
 msgnum:1' 1 $IMFILECHECKTIMEOUT
echo ' msgnum:2
 msgnum:3' >> $RSYSLOG_DYNNAME.input

# we now do a stop and restart of rsyslog. This checks that everything
# works across restarts.
shutdown_when_empty # shut down rsyslogd when done processing messages
wait_shutdown    # we need to wait until rsyslogd is finished!

#echo DROPPING YOU TO BASH!
#bash

startup

# new data
echo ' msgnum:4' >> $RSYSLOG_DYNNAME.input
content_check_with_count 'msgnum:2
 msgnum:3
 msgnum:4' 1 $IMFILECHECKTIMEOUT

echo ' msgnum:5
 msgnum:6' >> $RSYSLOG_DYNNAME.input
content_check_with_count 'msgnum:5
 msgnum:6' 1 $IMFILECHECKTIMEOUT

shutdown_when_empty # shut down rsyslogd when done processing messages
wait_shutdown    # we need to wait until rsyslogd is finished!
exit_test
