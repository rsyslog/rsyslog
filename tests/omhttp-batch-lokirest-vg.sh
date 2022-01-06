#!/bin/bash
export USE_VALGRIND="YES"
# export RS_TEST_VALGRIND_EXTRA_OPTS="--keep-debuginfo=yes --leak-check=full"

source ${srcdir:=.}/omhttp-batch-lokirest.sh
