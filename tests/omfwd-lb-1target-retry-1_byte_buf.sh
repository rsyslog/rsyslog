#!/bin/bash
# This wrapper retries the shared skeleton once more when TCP timing races
# trigger the known omfwd load-balancer flake. The second attempt keeps the
# same 1-byte-buffer scenario; it only avoids failing the suite on one unlucky
# receiver-close/reconnect timing window.
: "${srcdir:=.}"

export OMFWD_IOBUF_SIZE=10 # triggers edge cases
skeleton="$srcdir/omfwd-lb-1target-retry-test_skeleton.sh"
max_attempts=2
attempt=1
result=1

while [ "$attempt" -le "$max_attempts" ]; do
    if [ "$attempt" -gt 1 ]; then
        echo "Retrying omfwd 1-byte load-balancer test (attempt $attempt of $max_attempts) to mitigate TCP buffer timing races."
    fi

    if [ "$attempt" -lt "$max_attempts" ]; then
        TESTBENCH_SUPPRESS_FAIL_MARKER=YES "$skeleton"
    else
        "$skeleton"
    fi
    result=$?
    if [ "$result" -eq 0 ]; then
        if [ "$attempt" -gt 1 ]; then
            echo "omfwd 1-byte load-balancer test passed on retry."
        fi
        exit 0
    fi

    attempt=$((attempt + 1))
done

echo "omfwd 1-byte load-balancer test failed after $max_attempts attempts."
exit "$result"
