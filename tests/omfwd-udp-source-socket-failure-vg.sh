#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Full leak checking detects target addrinfo lists lost by repeated resume
# attempts when explicit-source socket creation fails. The common test also
# requires exact valid-source delivery and suspended-action fallback controls.
export USE_VALGRIND=YES
. ${srcdir:=.}/omfwd-udp-source-socket-failure.sh
