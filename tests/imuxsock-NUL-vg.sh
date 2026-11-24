#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# Valgrind coverage for imuxsock-NUL.sh. The base test checks exact sanitized
# output; this wrapper also detects the uninitialized-byte reads/writes reported
# in issue #4941 without combining Valgrind with TSAN.
export USE_VALGRIND="YES"
if [ -z "${srcdir+x}" ] && [ ! -f ./diag.sh ] && [ -f "$(dirname "$0")/diag.sh" ]; then
  cd "$(dirname "$0")" || exit 1
fi
. "${srcdir:=.}/imuxsock-NUL.sh"
