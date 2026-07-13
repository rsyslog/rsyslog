#!/bin/bash
# Crash after reserved segment creation; restart must adopt it safely.
export SEGDISK_FAULT_POINT=segment-created
. ${srcdir:=.}/testsuites/segmented-diskqueue-crash-driver.sh
