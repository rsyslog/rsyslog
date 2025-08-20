#!/bin/bash
# Debug version of omfwd-lb-1target-retry-full_buf.sh
# Added enhanced logging and diagnostics
export OMFWD_IOBUF_SIZE=0 # full buffer size

# Enable verbose rsyslog debugging
export RSYSLOG_DEBUG="debug"
export RSYSLOG_DEBUGLOG="$RSYSLOG_DYNNAME.rsyslog.debug"

# Set the test name for logging
export TEST_NAME="omfwd-lb-1target-retry-full_buf-debug"

# Create a wrapper around the skeleton test
. ${srcdir:=.}/diag.sh init

# Log system information
echo "=== System Information ===" | tee -a $RSYSLOG_DYNNAME.test.debug
uname -a | tee -a $RSYSLOG_DYNNAME.test.debug
cat /proc/cpuinfo | grep -E "model name|processor" | head -4 | tee -a $RSYSLOG_DYNNAME.test.debug
free -h | tee -a $RSYSLOG_DYNNAME.test.debug
echo "=== End System Information ===" | tee -a $RSYSLOG_DYNNAME.test.debug

# Run the skeleton test with timing information
echo "Test started at: $(date +%Y-%m-%d\ %H:%M:%S.%N)" | tee -a $RSYSLOG_DYNNAME.test.debug

# Source the skeleton but capture timing
time source ${srcdir:-.}/omfwd-lb-1target-retry-test_skeleton.sh 2>&1 | tee -a $RSYSLOG_DYNNAME.test.debug

TEST_RESULT=${PIPESTATUS[0]}

echo "Test ended at: $(date +%Y-%m-%d\ %H:%M:%S.%N)" | tee -a $RSYSLOG_DYNNAME.test.debug

# If test failed, gather additional diagnostics
if [ $TEST_RESULT -ne 0 ]; then
    echo "=== Test Failed - Gathering Diagnostics ===" | tee -a $RSYSLOG_DYNNAME.test.debug
    
    # Check message counts
    echo "Total lines in output log:" | tee -a $RSYSLOG_DYNNAME.test.debug
    wc -l $RSYSLOG_OUT_LOG | tee -a $RSYSLOG_DYNNAME.test.debug
    
    # Check for duplicate messages
    echo "Checking for duplicate messages:" | tee -a $RSYSLOG_DYNNAME.test.debug
    sort $RSYSLOG_OUT_LOG | uniq -d | head -20 | tee -a $RSYSLOG_DYNNAME.test.debug
    
    # Check message sequence gaps
    echo "First 10 messages:" | tee -a $RSYSLOG_DYNNAME.test.debug
    sort -n $RSYSLOG_OUT_LOG | head -10 | tee -a $RSYSLOG_DYNNAME.test.debug
    
    echo "Last 10 messages:" | tee -a $RSYSLOG_DYNNAME.test.debug
    sort -n $RSYSLOG_OUT_LOG | tail -10 | tee -a $RSYSLOG_DYNNAME.test.debug
    
    # Check rsyslog error messages
    echo "Rsyslog stderr output:" | tee -a $RSYSLOG_DYNNAME.test.debug
    if [ -f $RSYSLOG_DYNNAME.stderr ]; then
        tail -50 $RSYSLOG_DYNNAME.stderr | tee -a $RSYSLOG_DYNNAME.test.debug
    fi
    
    # Save all debug files
    echo "=== Preserving debug files ===" | tee -a $RSYSLOG_DYNNAME.test.debug
    for f in $RSYSLOG_DYNNAME.*; do
        echo "File: $f" | tee -a $RSYSLOG_DYNNAME.test.debug
    done
fi

exit $TEST_RESULT