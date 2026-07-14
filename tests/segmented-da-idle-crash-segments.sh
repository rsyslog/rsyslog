#!/bin/bash
# Crash after all segment deletions have been synchronized.
export SEGDISK_IDLE_FAULT_POINT=idle-segments-unlinked
. ${srcdir:=.}/testsuites/segmented-da-idle-crash-driver.sh
