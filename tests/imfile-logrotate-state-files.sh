#!/bin/bash
. $srcdir/diag.sh check-inotify-only
. ${srcdir:=.}/diag.sh init
export TESTMESSAGES=1000
export RETRIES=50
export TESTMESSAGESFULL=$((3 * $TESTMESSAGES - 1))
# export RSYSLOG_DEBUG="debug nostdout noprintmutexaction"
# export RSYSLOG_DEBUGLOG="log"

generate_conf
add_conf '
$WorkDirectory '$RSYSLOG_DYNNAME'.spool

# Enable debug for state file operations
global(
	debug.whitelist="on"
	debug.files=["imfile.c"]
)

module(	load="../plugins/imfile/.libs/imfile"
	mode="inotify"
)

input(type="imfile"
	File="./'$RSYSLOG_DYNNAME'.input.log"
	Tag="file:"
	Severity="error"
	Facility="local7"
	addMetadata="on"
	PersistStateInterval="100"
	deleteStateOnFileMove="on"
)

$template outfmt,"%msg:F,58:2%\n"
if $msg contains "msgnum:" then
 action(
   type="omfile"
   file=`echo $RSYSLOG_OUT_LOG`
   template="outfmt"
 )
'

print_state_files() {
	local round=$1
	echo "=== State files after round $round ==="
	if [ -d "$RSYSLOG_DYNNAME.spool" ]; then
		echo "State files in spool directory:"
		local state_count=$(ls -1 "$RSYSLOG_DYNNAME.spool/"imfile-state* 2>/dev/null | wc -l)
		echo "Total state files found: $state_count"

		if [ $state_count -eq 0 ]; then
			echo "No state files found"
		else
			ls -la "$RSYSLOG_DYNNAME.spool/"imfile-state* 2>/dev/null
			echo ""

			for statefile in "$RSYSLOG_DYNNAME.spool/"imfile-state*; do
				if [ -f "$statefile" ]; then
					echo "State file: $(basename "$statefile")"
					echo "  Size: $(stat -c%s "$statefile" 2>/dev/null || echo "unknown") bytes"
					echo "  Modified: $(stat -c%y "$statefile" 2>/dev/null || echo "unknown")"
					echo "  Contents:"
					if [ -r "$statefile" ]; then
						cat "$statefile" 2>/dev/null | jq . 2>/dev/null | sed 's/^/    /' || {
							echo "    Raw contents (not valid JSON):"
							cat "$statefile" 2>/dev/null | sed 's/^/    /'
						}
					else
						echo "    Cannot read file (permissions?)"
					fi
					echo ""
				fi
			done
		fi
	else
		echo "Spool directory does not exist"
	fi
	echo ""
}

print_inode_info() {
	local round=$1
	echo "=== Inode information after round $round ==="
	for file in "$RSYSLOG_DYNNAME".input.log*; do
		if [ -f "$file" ]; then
			local inode=$(stat -c%i "$file" 2>/dev/null || echo "N/A")
			local size=$(stat -c%s "$file" 2>/dev/null || echo "N/A")
			echo "File: $file - Inode: $inode - Size: $size bytes"
		fi
	done
	echo ""
}

write_log_lines() {
	local round=$1
	local start_num=$((($round - 1) * $TESTMESSAGES))
	echo "Writing $TESTMESSAGES log lines for round $round (starting from msgnum $start_num)"
	./inputfilegen -m $TESTMESSAGES -i $start_num >> "$RSYSLOG_DYNNAME.input.log"
}

rotate_logs() {
	echo "Rotating logs..."
	if [ -f "$RSYSLOG_DYNNAME.input.log.3" ]; then
		echo "  Removing $RSYSLOG_DYNNAME.input.log.3"
		rm -f "$RSYSLOG_DYNNAME.input.log.3"
	fi
	if [ -f "$RSYSLOG_DYNNAME.input.log.2" ]; then
		echo "  Moving $RSYSLOG_DYNNAME.input.log.2 to $RSYSLOG_DYNNAME.input.log.3"
		mv "$RSYSLOG_DYNNAME.input.log.2" "$RSYSLOG_DYNNAME.input.log.3"
	fi
	if [ -f "$RSYSLOG_DYNNAME.input.log.1" ]; then
		echo "  Moving $RSYSLOG_DYNNAME.input.log.1 to $RSYSLOG_DYNNAME.input.log.2"
		mv "$RSYSLOG_DYNNAME.input.log.1" "$RSYSLOG_DYNNAME.input.log.2"
	fi
	if [ -f "$RSYSLOG_DYNNAME.input.log" ]; then
		echo "  Moving $RSYSLOG_DYNNAME.input.log to $RSYSLOG_DYNNAME.input.log.1"
		mv "$RSYSLOG_DYNNAME.input.log" "$RSYSLOG_DYNNAME.input.log.1"
	fi
}

# Create initial empty input file
touch "$RSYSLOG_DYNNAME.input.log"

startup
echo "Started rsyslog, waiting 1 second for initialization..."
./msleep 1000

# Perform 3 rounds of write->rotate cycle
for round in 1 2 3; do
	echo ""
	echo "=== ROUND $round ==="

	# Write log lines
	write_log_lines $round

	# Print inode information
	print_inode_info "round $round (after write)"

	# Wait for processing
	echo "Waiting 1 second for processing..."
	./msleep 1000

	# Rotate logs
	rotate_logs

	# Wait after rotation
	echo "Waiting 1 second after rotation..."
	./msleep 1000

	# Check state files
	print_state_files $round

	# Print inode information after rotation
	print_inode_info "round $round (after rotation)"
done

# Verify we got the expected number of messages
expected_messages=$((3 * $TESTMESSAGES))
wait_file_lines $RSYSLOG_OUT_LOG $expected_messages $RETRIES
shutdown_when_empty
wait_shutdown

seq_check 0 $TESTMESSAGESFULL
print_state_files "after shutdown"

# Validate that there is exactly one state file after shutdown
state_count=$(ls -1 "$RSYSLOG_DYNNAME.spool/"imfile-state* 2>/dev/null | wc -l)
if [ $state_count -gt 1 ]; then
	echo "FAIL: Multiple state files found ($state_count) - expected max 1"
	error_exit 1
fi

# Cleanup
echo ""
echo "=== CLEANUP ==="
echo "Removing generated files..."
rm -f "$RSYSLOG_DYNNAME".input.log*
rm -rf "$RSYSLOG_DYNNAME.spool"
echo "Cleanup completed"

exit_test
