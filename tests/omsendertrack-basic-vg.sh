#!/bin/bash
set -x
pwd
ls -l omsender*sh
echo srcdir: $srcdir
export USE_VALGRIND="YES"
. ${srcdir:-.}/omsendertrack-basic.sh
