#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
#
# Copyright 2026 Jérémie Jourdin.
#
# A frame larger than the send buffer whose TLS write stays deferred past the
# bounded wait: the retried batch may write other frames to that connection
# first, so omfwd drops it and the pool fails over. Nothing may be lost;
# messages may repeat.
export OMFWD_FRAME_PAD=17000
export NUMMESSAGES=200
export OMFWD_HASH_RETRY_COUNT=105
export OMFWD_HASH_RETRY_DUPS_OK=1
export OMFWD_HASH_EXPECT_DROP=1
. ${srcdir:=.}/omfwd-hash-tls-retry.sh
