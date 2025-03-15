#!/bin/bash
set -x
pwd
ls -l omsender*sh
echo srcdir: $srcdir
export USE_VALGRIND="YES"
source ${srcdir:-.}/omsendertrack-basic.sh
