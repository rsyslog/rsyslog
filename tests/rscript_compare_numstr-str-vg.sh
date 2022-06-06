#!/bin/bash
export USE_VALGRIND="YES"
export LOWER_VAL='"1"'
export HIGHER_VAL='"abc"'
source ${srcdir:-.}/rscript_compare-common.sh
