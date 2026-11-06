#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Run exact 0/1/2 trailing-LF and empty-MSG controls under Valgrind.
export USE_VALGRIND="YES"
. ${srcdir:=.}/smfile-final-lf.sh
