#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
export USE_VALGRIND="YES"
export LOWER_VAL='"1"'
export HIGHER_VAL='"abc"'
. ${srcdir:-.}/rscript_compare-common.sh
