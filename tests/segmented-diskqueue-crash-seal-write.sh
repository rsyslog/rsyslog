#!/bin/bash
# Crash after writing a seal footer but before rename; restart salvages the tail.
export SEGDISK_FAULT_POINT=seal-written
. ${srcdir:=.}/testsuites/segmented-diskqueue-crash-driver.sh
