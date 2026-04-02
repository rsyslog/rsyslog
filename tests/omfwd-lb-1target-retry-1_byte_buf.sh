#!/bin/bash
export OMFWD_IOBUF_SIZE=10 # triggers edge cases
. ${srcdir:-.}/omfwd-lb-1target-retry-test_skeleton.sh
