# Debugging Guide for omfwd-lb-1target-retry-full_buf.sh Test Failures

## Overview
This test frequently fails on slow or resource-constrained systems (CentOS 7.5, ARM-based systems) due to message loss during connection drops.

## Root Causes

### 1. Timing Windows During Connection Drops
- The minitcpsrvr simulates server death by dropping connections
- There's a race condition between:
  - Connection drop detection
  - Buffer flushing
  - Resume timer expiration
  - Message requeuing

### 2. Buffer Management Issues
- With `OMFWD_IOBUF_SIZE=0` (full buffer), the system uses maximum buffer sizes
- On slow systems, large buffers can cause:
  - Memory pressure
  - Slower buffer operations
  - Increased chance of message loss during state transitions

### 3. Resource Constraints
- Test sends 20,000 messages rapidly
- Slow CPUs can't process messages fast enough
- Limited network buffers on embedded systems

## Debugging Steps

### 1. Run Enhanced Debug Version
```bash
# Run the debug version of the test
./omfwd-lb-1target-retry-full_buf-debug.sh

# Check the debug output
cat rsyslog.out_log.test.debug
cat rsyslog.out_log.rsyslog.debug
```

### 2. Analyze Failed Test Output
```bash
# Use the analysis script on failed test output
./analyze-omfwd-failure.sh rsyslog.out_log

# This will show:
# - Total messages received vs expected
# - Missing message ranges
# - Duplicate messages
# - Distribution of losses
```

### 3. Check System Resources During Test
```bash
# Monitor during test execution
# Terminal 1:
./omfwd-lb-1target-retry-full_buf.sh

# Terminal 2:
watch -n 1 'ps aux | grep -E "(rsyslog|minitcp)" | grep -v grep'
watch -n 1 'netstat -an | grep ESTABLISHED | wc -l'
```

### 4. Enable Verbose rsyslog Debugging
```bash
# Set environment variables before running test
export RSYSLOG_DEBUG="debug"
export RSYSLOG_DEBUGLOG="rsyslog.debug.log"

# Run test
./omfwd-lb-1target-retry-full_buf.sh

# Analyze debug log for connection state changes
grep -E "(suspend|resume|connection|retry|buffer)" rsyslog.debug.log
```

### 5. Increase Test Tolerances for Slow Systems
```bash
# Set environment variable to enable slow system mode
export SLOW_SYSTEM=1

# This will:
# - Increase timeouts
# - Add delays between message batches
# - Increase allowed message loss threshold

./omfwd-lb-1target-retry-full_buf.sh
```

## Common Failure Patterns

### Pattern 1: Burst Loss During Connection Drop
- **Symptom**: Large consecutive range of messages missing (e.g., 8000-9000)
- **Cause**: Messages sent while connection is down and buffers overflow
- **Fix**: Increase queue sizes, add grace period

### Pattern 2: Random Message Loss
- **Symptom**: Individual messages missing throughout the range
- **Cause**: CPU can't keep up with message rate
- **Fix**: Batch message injection, add delays

### Pattern 3: Loss at Test End
- **Symptom**: Last few hundred messages missing
- **Cause**: Shutdown occurs before all messages processed
- **Fix**: Increase shutdown timeout, ensure proper queue draining

## Recommended Fixes

### 1. Test-Level Fixes
- Use improved test skeleton with better timing
- Detect slow systems and adjust parameters
- Batch message injection to avoid overwhelming system

### 2. omfwd Module Fixes
- Add connection grace period to avoid immediate suspension
- Improve buffer management during state transitions
- Ensure messages in send buffer are requeued on connection failure

### 3. Configuration Tuning
```
action(
    type="omfwd"
    # ... other params ...
    queue.type="LinkedList"        # Better for large queues
    queue.size="50000"            # Larger queue for buffering
    queue.saveOnShutdown="on"     # Don't lose messages on shutdown
    queue.timeoutEnqueue="10000"  # Wait longer before dropping
    pool.resumeInterval="1"       # Faster retry
    connection.graceperiod="2"    # Grace period before suspend
)
```

## Testing the Fixes

1. Run on the same slow systems that showed failures
2. Monitor message loss with different configurations
3. Verify no regression on fast systems
4. Check that legitimate connection failures still work correctly

## Further Analysis Tools

### tcpdump/Wireshark
```bash
# Capture network traffic during test
sudo tcpdump -i lo -w omfwd-test.pcap tcp port <minitcpsrvr-port>

# Analyze for:
# - Connection resets
# - Retransmissions
# - Buffer sizes
```

### strace
```bash
# Trace rsyslog system calls
strace -f -o rsyslog.strace -p <rsyslog-pid>

# Look for:
# - Failed send() calls
# - EAGAIN/EWOULDBLOCK errors
# - Buffer operations
```

### Performance Profiling
```bash
# Use perf to identify bottlenecks
perf record -g -p <rsyslog-pid>
perf report

# Look for hot spots in:
# - Buffer management
# - Queue operations
# - Network I/O
```