#!/bin/bash
# Crash after commit-frontier publication and recover without loss.
export SEGDISK_FAULT_POINT=commit-published
. ${srcdir:=.}/testsuites/segmented-diskqueue-crash-driver.sh
