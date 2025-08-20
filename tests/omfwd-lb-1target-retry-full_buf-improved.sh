#!/bin/bash
# Improved version of omfwd-lb-1target-retry-full_buf.sh
# with better handling for slow/resource-constrained systems
export OMFWD_IOBUF_SIZE=0 # full buffer size

# Detect if we're on a slow system
SLOW_SYSTEM=0
if [ -f /proc/cpuinfo ]; then
    CPU_COUNT=$(grep -c ^processor /proc/cpuinfo)
    CPU_SPEED=$(grep "cpu MHz" /proc/cpuinfo | head -1 | awk '{print int($4)}')
    
    # Consider system slow if less than 2 CPUs or CPU speed < 1500 MHz
    if [ "$CPU_COUNT" -lt 2 ] || [ "$CPU_SPEED" -lt 1500 ]; then
        SLOW_SYSTEM=1
        echo "Detected slow system: CPUs=$CPU_COUNT, Speed=${CPU_SPEED}MHz"
    fi
fi

# Also check if it's ARM architecture
if uname -m | grep -q "arm\|aarch"; then
    SLOW_SYSTEM=1
    echo "Detected ARM architecture - treating as slow system"
fi

# Export for use in skeleton
export SLOW_SYSTEM

# Use the improved skeleton if available, otherwise fallback to original
if [ -f "${srcdir:-.}/omfwd-lb-1target-retry-test_skeleton-improved.sh" ]; then
    source ${srcdir:-.}/omfwd-lb-1target-retry-test_skeleton-improved.sh
else
    source ${srcdir:-.}/omfwd-lb-1target-retry-test_skeleton.sh
fi