#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
export USE_VALGRIND="YES"
#export RS_TEST_VALGRIND_EXTRA_OPTS="--leak-check=full --show-leak-kinds=all"
. ${srcdir:-.}/imhttp-post-payload-compress.sh
