#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# A named source actually binds both IPv6 and IPv4, in that order. Exactly one
# IPv4-only target delivery must succeed without cross-family socket retries.
# The observer records successful real UDP binds; IPv6 capability skips only
# this additional case, preserving the numeric-IPv4 regressions elsewhere.
export OMFWD_FAMILY_DUAL_SOURCE=1
. ${srcdir:=.}/omfwd-udp-source-family.sh
