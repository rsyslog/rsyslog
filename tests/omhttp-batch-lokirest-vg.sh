#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
export USE_VALGRIND="YES"
# export RS_TEST_VALGRIND_EXTRA_OPTS="--keep-debuginfo=yes --leak-check=full"

. ${srcdir:=.}/omhttp-batch-lokirest.sh
