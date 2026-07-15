#!/bin/bash
# Crash immediately after the durable empty-state publication.
export SEGDISK_IDLE_FAULT_POINT=idle-empty-published
. ${srcdir:=.}/testsuites/segmented-da-idle-crash-driver.sh
