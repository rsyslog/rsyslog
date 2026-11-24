#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Run the exact 0/1/2 trailing-LF output oracle under Valgrind.
export USE_VALGRIND="YES"
. ${srcdir:=.}/smtradfile-final-lf.sh
