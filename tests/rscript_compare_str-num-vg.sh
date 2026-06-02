#!/bin/bash
export USE_VALGRIND="YES"
export LOWER_VAL='"-"'
export HIGHER_VAL='1'
. ${srcdir:-.}/rscript_compare-common.sh
