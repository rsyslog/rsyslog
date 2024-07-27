#!/bin/bash
export OMFWD_IOBUF_SIZE=1000 # triggers edge cases
source ${srcdir:-.}/omfwd-lb-1target-retry-test_skeleton-TargetFail.sh
