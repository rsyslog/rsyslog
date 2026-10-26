#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
#
# Copyright 2026 Jérémie Jourdin.
#
# Same as omfwd-hash-tls-retry.sh, with every frame larger than omfwd's send
# buffer. Such a frame is written directly, and with octet-counted framing
# tcpclt builds it in a new buffer on every call, so the repeat after a
# deferral renders it again: the fixture checks that the repeat still comes
# from the deferred write's buffer and offers the deferred bytes.
export OMFWD_FRAME_PAD=17000
export OMFWD_FRAMING=octet-counted
export NUMMESSAGES=200
. ${srcdir:=.}/omfwd-hash-tls-retry.sh
