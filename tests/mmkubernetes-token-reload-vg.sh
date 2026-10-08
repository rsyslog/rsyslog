#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# valgrind variant of mmkubernetes-token-reload.sh - see that script for details.
export USE_VALGRIND="YES"
. ${srcdir:=.}/mmkubernetes-token-reload.sh
