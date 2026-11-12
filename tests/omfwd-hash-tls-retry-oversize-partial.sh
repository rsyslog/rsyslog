#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
#
# Copyright 2026 Jérémie Jourdin.
#
# Same as omfwd-hash-tls-retry-oversize.sh, with the deferred write of a
# frame larger than the send buffer preceded by a short write of its first
# 4096 bytes. The repeat must continue after those bytes, from the
# per-target copy of the frame.
export OMFWD_FRAME_PAD=17000
export OMFWD_FRAMING=octet-counted
export NUMMESSAGES=200
export OMFWD_HASH_RETRY_PARTIAL=4096
. ${srcdir:=.}/omfwd-hash-tls-retry.sh
