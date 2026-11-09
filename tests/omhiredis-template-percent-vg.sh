#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# added 2026-11-09 by Rainer Gerhards, released under ASL 2.0
## Uncomment for debugging
#export RS_REDIR=-d

export USE_VALGRIND="YES"
. ${srcdir:=.}/omhiredis-template-percent.sh
