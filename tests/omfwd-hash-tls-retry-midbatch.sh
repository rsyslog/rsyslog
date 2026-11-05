#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
#
# Copyright 2026 Jérémie Jourdin.
#
# Same as omfwd-hash-tls-retry.sh, with a send buffer too small for two
# frames: once a batch holds two frames for one target, the deferred TLS send
# is the buffer-full flush inside the batch rather than the end-of-batch
# flush. A first batch with a single frame per target would still defer at
# the end-of-batch flush, which the other test already covers.
export OMFWD_IOBUFFER=64
. ${srcdir:=.}/omfwd-hash-tls-retry.sh
