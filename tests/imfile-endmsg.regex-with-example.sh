#!/bin/bash
# This is part of the rsyslog testbench, licensed under ASL 2.0
# This test verifies imfile endmsg.regex message assembly for CRI-O and JSON
# container log fragments, including metadata and normalized multiline output.
# The oracle is six completed output records: two completed records from each
# input in the first batch, then one multiline record from each input after the
# final terminator arrives. The trailing partial records must remain buffered.
. ${srcdir:=.}/diag.sh init
. $srcdir/diag.sh check-inotify
# The final multiline record can be delayed heavily when the full test suite
# runs at extreme concurrency. This timeout bounds that wait while preserving
# the exact six-record oracle below.
export IMFILECHECKTIMEOUT="180"
export IMFILELASTINPUTLINES="6"

mkdir $RSYSLOG_DYNNAME.statefiles
generate_conf
add_conf '
module(load="../plugins/imfile/.libs/imfile"
      statefile.directory="'${RSYSLOG_DYNNAME}'.statefiles")
module(load="../plugins/mmnormalize/.libs/mmnormalize")

input(type="imfile"
      File="./'$RSYSLOG_DYNNAME'.*.input"
      Tag="file:" addMetadata="on" escapelf="off"
      endmsg.regex="(^[^ ]+ (stdout|stderr) F )|(\\n\"}$)")

if $msg startswith "{" then {
	action(type="mmnormalize" rulebase="'$srcdir/imfile-endmsg.regex.json.rulebase'")
	foreach ($.ii in $!multilinejson) do {
		if strlen($!@timestamp) == 0 then {
			set $!@timestamp = $.ii!time;
		}
		if strlen($!stream) == 0 then {
			set $!stream = $.ii!stream;
		}
		if strlen($!log) == 0 then {
			set $!log = $.ii!log;
		} else {
			reset $!log = $!log & $.ii!log;
		}
	}
	unset $!multilinejson;
} else {
	action(type="mmnormalize" rulebase="'$srcdir/imfile-endmsg.regex.crio.rulebase'")
	foreach ($.ii in $!multilinecrio) do {
		if strlen($!@timestamp) == 0 then {
			set $!@timestamp = $.ii!time;
		}
		if strlen($!stream) == 0 then {
			set $!stream = $.ii!stream;
		}
		if strlen($!log) == 0 then {
			set $!log = $.ii!log;
		} else {
			reset $!log = $!log & $.ii!log;
		}
	}
	unset $!multilinecrio;
}

template(name="outfmt" type="list") {
  property(name="$!all-json-plain")
  constant(value="\n")
}

if $msg contains "msgnum:" then
 action(
   type="omfile"
   file=`echo $RSYSLOG_OUT_LOG`
   template="outfmt"
 )
'
if [ "${USE_VALGRIND:-false}" == "true" ] ; then
	startup_vg
else
	startup
fi
if [ -n "${USE_GDB:-}" ] ; then
	echo attach gdb here
	sleep 54321 || :
fi

# write the beginning of the file
echo 'date stdout P msgnum:0' > $RSYSLOG_DYNNAME.crio.input
echo '{"time":"date", "stream":"stdout", "log":"msgnum:0"}' > $RSYSLOG_DYNNAME.json.input
echo 'date stdout F msgnum:1' >> $RSYSLOG_DYNNAME.crio.input
printf '%s\n' '{"time":"date", "stream":"stdout", "log":"msgnum:1\n"}' >> $RSYSLOG_DYNNAME.json.input
echo 'date stdout F msgnum:2' >> $RSYSLOG_DYNNAME.crio.input
printf '%s\n' '{"time":"date", "stream":"stdout", "log":"msgnum:2\n"}' >> $RSYSLOG_DYNNAME.json.input

if [ -n "${USE_GDB:-}" ] ; then
	sleep 54321 || :
fi

wait_file_lines $RSYSLOG_OUT_LOG 4 $IMFILECHECKTIMEOUT

echo 'date stdout P msgnum:3' >> $RSYSLOG_DYNNAME.crio.input
echo '{"time":"date", "stream":"stdout", "log":"msgnum:3"}' >> $RSYSLOG_DYNNAME.json.input
echo 'date stdout P msgnum:4' >> $RSYSLOG_DYNNAME.crio.input
echo '{"time":"date", "stream":"stdout", "log":"msgnum:4"}' >> $RSYSLOG_DYNNAME.json.input
echo 'date stdout P msgnum:5' >> $RSYSLOG_DYNNAME.crio.input
echo '{"time":"date", "stream":"stdout", "log":"msgnum:5"}' >> $RSYSLOG_DYNNAME.json.input

if [ -n "${USE_GDB:-}" ] ; then
	sleep 54321 || :
fi

# The second batch is intentionally incomplete. Drain the queue before appending
# the terminators so the test checks imfile's cross-append multiline state
# instead of depending on a fixed sleep under load.
wait_queueempty

echo 'date stdout F msgnum:6' >> $RSYSLOG_DYNNAME.crio.input
printf '%s\n' '{"time":"date", "stream":"stdout", "log":"msgnum:6\n"}' >> $RSYSLOG_DYNNAME.json.input
echo 'date stdout P msgnum:7' >> $RSYSLOG_DYNNAME.crio.input
echo '{"time":"date", "stream":"stdout", "log":"msgnum:7"}' >> $RSYSLOG_DYNNAME.json.input

wait_file_lines $RSYSLOG_OUT_LOG $IMFILELASTINPUTLINES $IMFILECHECKTIMEOUT

shutdown_when_empty # shut down rsyslogd when done processing messages
if [ "${USE_VALGRIND:-false}" == "true" ] ; then
	wait_shutdown_vg
	check_exit_vg
else
	wait_shutdown    # we need to wait until rsyslogd is finished!
fi

## check if we have the correct number of messages

NUMLINES=$(wc -l $RSYSLOG_OUT_LOG | awk '{print $1}' 2>/dev/null)

rc=0
if [ ! $NUMLINES -eq $IMFILELASTINPUTLINES ]; then
  echo "ERROR: expecting $IMFILELASTINPUTLINES lines, got $NUMLINES"
  rc=1
fi

## check if all the data we expect to get in the file is there

for string in "metadata.*filename.*json[.]input.*msgnum:0msgnum:1" \
	"metadata.*filename.*crio[.]input.*msgnum:0msgnum:1" \
	"metadata.*filename.*json[.]input.*msgnum:2" \
	"metadata.*filename.*crio[.]input.*msgnum:2" \
	"metadata.*filename.*json[.]input.*msgnum:3msgnum:4msgnum:5msgnum:6" \
	"metadata.*filename.*crio[.]input.*msgnum:3msgnum:4msgnum:5msgnum:6" ; do
	if grep "$string" $RSYSLOG_OUT_LOG > /dev/null 2>&1 ; then
		: # ok
	else
		echo "Error: the expected string '$string' not found"
		rc=1
	fi
done

if [ $rc -ne 0 ]; then
    cat $RSYSLOG_OUT_LOG
    exit 1
fi

## if we got here, all is good :)

exit_test
