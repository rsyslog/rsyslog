#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
export USE_VALGRIND="YES"
export LOWER_VAL='"a"'
export HIGHER_VAL='"b"'
. ${srcdir:-.}/rscript_compare-common.sh
