#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Crash after publishing the head before deletion; leftovers are idempotent.
export SEGDISK_FAULT_POINT=predelete-published
. ${srcdir:=.}/testsuites/segmented-diskqueue-crash-driver.sh
