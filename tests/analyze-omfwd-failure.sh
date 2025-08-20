#!/bin/bash
# Script to analyze omfwd test failures
# Usage: ./analyze-omfwd-failure.sh <test-output-file>

if [ $# -ne 1 ]; then
    echo "Usage: $0 <test-output-file>"
    exit 1
fi

OUTPUT_FILE="$1"

if [ ! -f "$OUTPUT_FILE" ]; then
    echo "Error: File $OUTPUT_FILE not found"
    exit 1
fi

echo "=== Analyzing omfwd test failure ==="
echo "File: $OUTPUT_FILE"
echo

# Count total messages
TOTAL_MSGS=$(wc -l < "$OUTPUT_FILE")
echo "Total messages received: $TOTAL_MSGS"

# Check for duplicates
DUPLICATES=$(sort "$OUTPUT_FILE" | uniq -d | wc -l)
echo "Duplicate messages: $DUPLICATES"

if [ $DUPLICATES -gt 0 ]; then
    echo "First 10 duplicate messages:"
    sort "$OUTPUT_FILE" | uniq -d | head -10
fi

# Extract message numbers and analyze gaps
echo
echo "=== Message sequence analysis ==="
# Extract numbers from messages (assuming format contains number)
sort -n "$OUTPUT_FILE" > sorted_msgs.tmp

# Find first and last message numbers
FIRST_MSG=$(head -1 sorted_msgs.tmp)
LAST_MSG=$(tail -1 sorted_msgs.tmp)
echo "First message: $FIRST_MSG"
echo "Last message: $LAST_MSG"

# Create expected sequence and find missing
echo
echo "=== Missing message analysis ==="
awk '{print $1}' sorted_msgs.tmp | sort -n -u > actual_sequence.tmp

# Generate expected sequence (0 to 19999)
seq 0 19999 > expected_sequence.tmp

# Find missing messages
comm -23 expected_sequence.tmp actual_sequence.tmp > missing_messages.tmp
MISSING_COUNT=$(wc -l < missing_messages.tmp)

echo "Total missing messages: $MISSING_COUNT"
echo

if [ $MISSING_COUNT -gt 0 ]; then
    echo "Missing message ranges:"
    # Analyze missing messages to find ranges
    awk '
    BEGIN { start = -1; prev = -1 }
    {
        if (start == -1) {
            start = $1
            prev = $1
        } else if ($1 == prev + 1) {
            prev = $1
        } else {
            if (start == prev) {
                print start
            } else {
                print start "-" prev " (" (prev - start + 1) " messages)"
            }
            start = $1
            prev = $1
        }
    }
    END {
        if (start != -1) {
            if (start == prev) {
                print start
            } else {
                print start "-" prev " (" (prev - start + 1) " messages)"
            }
        }
    }
    ' missing_messages.tmp | head -20
    
    echo
    echo "Distribution of missing messages:"
    # Check if missing messages are clustered
    awk '{print int($1/1000) * 1000 "-" (int($1/1000) + 1) * 1000}' missing_messages.tmp | \
        sort | uniq -c | sort -nr | head -10
fi

# Cleanup
rm -f sorted_msgs.tmp actual_sequence.tmp expected_sequence.tmp missing_messages.tmp

echo
echo "=== Analysis complete ==="