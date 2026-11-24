#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
set -x
pwd
ls -l omsender*sh
echo srcdir: $srcdir
export USE_VALGRIND="YES"
. ${srcdir:-.}/omsendertrack-basic.sh
