#!/bin/bash
export USE_VALGRIND="YES"
export LOWER_VAL='"a"'
export HIGHER_VAL='"b"'
source ${srcdir:-.}/rscript_compare-common.sh
