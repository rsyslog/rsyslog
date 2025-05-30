#!/bin/bash
export USE_VALGRIND="YES"
export LOWER_VAL='"-"'
export HIGHER_VAL='"2"'
source ${srcdir:-.}/rscript_compare-common.sh
