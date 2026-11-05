#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
#
# Copyright 2026 Jérémie Jourdin.
#
# Same as omfwd-hash-tls-retry.sh, with the deferred end-of-batch flush
# preceded by a short write of its first 64 bytes, as a TLS driver that
# sends one record per call does. The repeat must continue after those
# bytes; starting over would send them twice.
export OMFWD_HASH_RETRY_PARTIAL=64
. ${srcdir:=.}/omfwd-hash-tls-retry.sh
