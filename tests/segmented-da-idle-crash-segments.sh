#!/bin/bash
# Crash after all segment deletions have been synchronized in two consecutive
# materialization cycles; final exact drain proves repeated cleanup recovery.
export SEGDISK_IDLE_FAULT_POINT=idle-segments-unlinked
export SEGDISK_IDLE_FAULT_REPEATS=2
. ${srcdir:=.}/testsuites/segmented-da-idle-crash-driver.sh
