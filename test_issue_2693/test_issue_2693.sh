#!/bin/bash
# Test for issue #2693: shutdown loop when queue.maxDiskSpace < queue.maxfilesize
# This test demonstrates the problem and verifies the fix

set -e

# Test configuration that should trigger the error
cat > test_issue_2693.conf << 'EOF'
# Test configuration for issue #2693
# This configuration sets queue.maxDiskSpace (1MB) smaller than queue.maxfilesize (2MB)
# which should cause the shutdown loop described in the issue

$ModLoad imuxsock
$ModLoad imklog

# Main message queue with problematic settings
$MainMsgQueueType LinkedList
$MainMsgQueueFileName mainQ
$MainMsgQueueMaxFileSize 2m
$MainMsgQueueMaxDiskSpace 1m
$MainMsgQueueWorkerThreads 1

# Action queue with problematic settings
$ActionQueueType LinkedList
$ActionQueueFileName actionQ
$ActionQueueMaxFileSize 2m
$ActionQueueMaxDiskSpace 1m
$ActionQueueWorkerThreads 1

# Simple action to generate some load
*.* /var/log/test.log
EOF

echo "Testing issue #2693: queue.maxDiskSpace < queue.maxfilesize validation"
echo "This test should show an error message about invalid queue configuration"

# Run rsyslog with the problematic configuration
# The fix should prevent rsyslog from starting with this invalid configuration
if ../rsyslog/rsyslogd -f test_issue_2693.conf -N1 2>&1 | grep -q "queue.maxDiskSpace.*is smaller than queue.maxFileSize"; then
    echo "SUCCESS: Issue #2693 is fixed - rsyslog correctly detects invalid configuration"
    echo "The error message indicates that queue.maxDiskSpace cannot be smaller than queue.maxFileSize"
    exit 0
else
    echo "FAILURE: Issue #2693 is not fixed - rsyslog should detect invalid configuration"
    echo "Expected error message about queue.maxDiskSpace being smaller than queue.maxFileSize"
    exit 1
fi