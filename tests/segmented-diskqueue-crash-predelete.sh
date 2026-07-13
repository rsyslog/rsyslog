#!/bin/bash
# Crash after publishing the head before deletion; leftovers are idempotent.
export SEGDISK_FAULT_POINT=predelete-published
. ${srcdir:=.}/testsuites/segmented-diskqueue-crash-driver.sh
