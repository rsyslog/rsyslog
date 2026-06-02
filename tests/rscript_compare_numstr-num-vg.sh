#!/bin/bash
export USE_VALGRIND="YES"
export LOWER_VAL='"1"'
export HIGHER_VAL='2'
. ${srcdir:-.}/rscript_compare-common.sh
