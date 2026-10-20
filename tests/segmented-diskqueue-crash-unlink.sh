#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Crash after a segment unlink; conservative state permits idempotent cleanup.
export SEGDISK_FAULT_POINT=segment-unlinked
. ${srcdir:=.}/testsuites/segmented-diskqueue-crash-driver.sh
