#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
export USE_VALGRIND="YES"
export LOWER_VAL='"-"'
export HIGHER_VAL='1'
. ${srcdir:-.}/rscript_compare-common.sh
