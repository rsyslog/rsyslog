#!/bin/bash
# Crash after state unlink, when the queue directory is empty.
export SEGDISK_IDLE_FAULT_POINT=idle-state-unlinked
. ${srcdir:=.}/testsuites/segmented-da-idle-crash-driver.sh
