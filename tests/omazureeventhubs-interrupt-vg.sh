#!/bin/bash
export USE_VALGRIND="YES"
export RS_TEST_VALGRIND_EXTRA_OPTS="--keep-debuginfo=yes --leak-check=full"
export EXTRA_VALGRIND_SUPPRESSIONS="--suppressions=omazureeventhubs.supp"
source ${srcdir:-.}/omazureeventhubs-interrupt.sh
