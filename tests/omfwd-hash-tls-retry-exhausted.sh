#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
#
# Copyright 2026 Jérémie Jourdin.
#
# Same as omfwd-hash-tls-retry.sh, with the TLS write deferred for longer than
# the bounded in-place retry. The action then suspends and the engine replays
# the batch: nothing may be lost; what the deferred target was handed, and
# what other targets had already sent for the batch, may arrive twice.
export OMFWD_HASH_RETRY_COUNT=105
export OMFWD_HASH_RETRY_DUPS_OK=1
. ${srcdir:=.}/omfwd-hash-tls-retry.sh
