#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# Run the real-daemon timestamp bounds/cursor regression under Valgrind.
export USE_VALGRIND=YES
. "${srcdir:=.}/imuxsock-timestamp-rewrite.sh"
