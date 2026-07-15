#!/bin/bash
# Crash after the empty segmented store directory has been removed.
export SEGDISK_IDLE_FAULT_POINT=idle-directory-removed
. ${srcdir:=.}/testsuites/segmented-da-idle-crash-driver.sh
