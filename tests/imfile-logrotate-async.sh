#!/bin/bash
# This is part of the rsyslog testbench, licensed under ASL 2.0
. $srcdir/diag.sh check-inotify-only
. ${srcdir:=.}/diag.sh init
check_command_available logrotate
export NUMMESSAGES=10000
export RETRIES=50
export ROTATE_STOP_VISIBLE_LINES=$((NUMMESSAGES - 1000))

# Uncomment fdor debuglogs
#export RSYSLOG_DEBUG="debug nostdout noprintmutexaction"
#export RSYSLOG_DEBUGLOG="$RSYSLOG_DYNNAME.debuglog"

# Write logrotate config file
echo '"./'$RSYSLOG_DYNNAME'.input*.log"
{
    su '"$(id -un)"' '"$(id -gn)"'
    #daily
    rotate 60
    missingok
    notifempty
    sharedscripts
    postrotate
    kill -HUP $(cat '$RSYSLOG_DYNNAME'.inputfilegen_pid)
    endscript
    #olddir /logs/old

}' > $RSYSLOG_DYNNAME.logrotate


generate_conf
add_conf '
$WorkDirectory '$RSYSLOG_DYNNAME'.spool

global( debug.whitelist="on"
	debug.files=["imfile.c", "stream.c"]
	)

module(load="../plugins/imfile/.libs/imfile" mode="inotify" PollingInterval="2")

input(type="imfile" File="./'$RSYSLOG_DYNNAME'.input*.log" Tag="file:"
	Severity="error" Facility="local7" addMetadata="on" reopenOnTruncate="on")

$template outfmt,"%msg:F,58:2%\n"
if $msg contains "msgnum:" then
   action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
'
startup

count_rotated_input_lines() {
	local count=0
	local lines
	local file

	for file in $RSYSLOG_DYNNAME.input.log.[0-9]*; do
		if [ -f "$file" ]; then
			lines=$(grep -c '^msgnum:' "$file")
			count=$((count + lines))
		fi
	done
	echo "$count"
}

count_input_lines() {
	local count=0
	local lines
	local file

	for file in $RSYSLOG_DYNNAME.input.log*; do
		if [ -f "$file" ]; then
			lines=$(grep -c '^msgnum:' "$file")
			count=$((count + lines))
		fi
	done
	echo "$count"
}

wait_inputfilegen_reopen() {
	local old_inode="$1"
	local new_inode
	local timeoutend=$(( $(date +%s) + RETRIES ))

	while true; do
		new_inode=$(ls -i "$RSYSLOG_DYNNAME.input.log" 2>/dev/null | awk '{print $1}')
		if [ -n "$new_inode" ] && [ "$new_inode" != "$old_inode" ]; then
			return
		fi
		if [ "$(date +%s)" -ge "$timeoutend" ]; then
			echo "inputfilegen did not reopen $RSYSLOG_DYNNAME.input.log after rotation"
			error_exit 1
		fi
		./msleep 20
	done
}

wait_input_nonempty() {
	local timeoutend=$(( $(date +%s) + RETRIES ))

	while true; do
		if [ -s "$RSYSLOG_DYNNAME.input.log" ]; then
			return
		fi
		if ! kill -0 "$INPUTFILEGEN_PID" 2>/dev/null; then
			echo "inputfilegen exited before $RSYSLOG_DYNNAME.input.log became non-empty"
			error_exit 1
		fi
		if [ "$(date +%s)" -ge "$timeoutend" ]; then
			echo "$RSYSLOG_DYNNAME.input.log did not become non-empty before rotation"
			error_exit 1
		fi
		./msleep 20
	done
}

rotate_and_wait_processed() {
	local old_inode
	local expected

	wait_input_nonempty
	if [ "$(count_input_lines)" -ge "$ROTATE_STOP_VISIBLE_LINES" ]; then
		echo "inputfilegen is in the final message range, skipping further rotation"
		return
	fi
	old_inode=$(ls -i "$RSYSLOG_DYNNAME.input.log" 2>/dev/null | awk '{print $1}')
	logrotate --state $RSYSLOG_DYNNAME.logrotate.state -f $RSYSLOG_DYNNAME.logrotate
	wait_inputfilegen_reopen "$old_inode"
	expected=$(count_rotated_input_lines)
	if [ "$expected" -gt 1 ]; then
		# A just-closed rotated file can expose one final buffered record
		# before imfile receives a later modify event. The exact final
		# 10000-line assertion below covers that record after the sentinel.
		wait_file_lines $RSYSLOG_OUT_LOG "$((expected - 1))" $RETRIES
	elif [ "$expected" -gt 0 ]; then
		wait_file_lines $RSYSLOG_OUT_LOG "$expected" $RETRIES
	fi
}

./inputfilegen -m $NUMMESSAGES -S 20 -B 25 -I 1000 -f $RSYSLOG_DYNNAME.input.log &
INPUTFILEGEN_PID=$!
echo "$INPUTFILEGEN_PID" > $RSYSLOG_DYNNAME.inputfilegen_pid

./msleep 1
rotate_and_wait_processed
echo ======================:
echo ROTATE 1	INPUT FILES:
ls -li $RSYSLOG_DYNNAME.input*
rotate_and_wait_processed
echo ======================:
echo ROTATE 2	INPUT FILES:
ls -li $RSYSLOG_DYNNAME.input*
rotate_and_wait_processed
echo ======================:
echo ROTATE 3	INPUT FILES:
ls -li $RSYSLOG_DYNNAME.input*
echo ======================:
echo ls ${RSYSLOG_DYNNAME}.spool:
ls -li ${RSYSLOG_DYNNAME}.spool
echo ======================:
echo FINAL	INPUT FILES:
ls -li $RSYSLOG_DYNNAME.input*

# generate more input after logrotate into new logfile
#./inputfilegen -m $TESTMESSAGES -i $TESTMESSAGES >> $RSYSLOG_DYNNAME.input.1.log
#ls -l $RSYSLOG_DYNNAME.input*

#msgcount=$((2* TESTMESSAGES))
#wait_file_lines $RSYSLOG_OUT_LOG $msgcount $RETRIES

if ! wait "$INPUTFILEGEN_PID"; then
	echo "inputfilegen failed"
	error_exit 1
fi

# Output extra information
echo ======================:
echo LINES:	$(wc -l $RSYSLOG_DYNNAME.input.log)
echo TAIL	$RSYSLOG_DYNNAME.input.log:
tail $RSYSLOG_DYNNAME.input.log
echo ""
echo LINES:	$(wc -l $RSYSLOG_DYNNAME.input.log.1)
echo TAIL	$RSYSLOG_DYNNAME.input.log.1:
tail $RSYSLOG_DYNNAME.input.log.1
echo ""
echo LINES:	$(wc -l $RSYSLOG_DYNNAME.input.log.2)
echo TAIL	$RSYSLOG_DYNNAME.input.log.2:
tail $RSYSLOG_DYNNAME.input.log.2
echo ""
echo LINES:	$(wc -l $RSYSLOG_DYNNAME.input.log.3)
echo TAIL	$RSYSLOG_DYNNAME.input.log.3:
tail $RSYSLOG_DYNNAME.input.log.3
echo ""

# inputfilegen buffers its named output stream while it idles after the final
# record. Once it exits, append an ignored line to force a post-close IN_MODIFY
# event so imfile drains records flushed only during generator shutdown. The
# final records may be in either the active file or a just-rotated file.
for file in $RSYSLOG_DYNNAME.input.log*; do
	if [ -f "$file" ]; then
		echo "inputfilegen-complete" >> "$file"
	fi
done
wait_file_lines

touch $RSYSLOG_DYNNAME.input.log
./msleep 1000

shutdown_when_empty
wait_shutdown
seq_check 
#seq_check 0 $TESTMESSAGESFULL
exit_test
