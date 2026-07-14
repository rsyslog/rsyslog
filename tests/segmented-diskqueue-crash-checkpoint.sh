#!/bin/bash
# Crash after an ordinary checkpoint publication and recover without loss.
export SEGDISK_FAULT_POINT=checkpoint-published
. ${srcdir:=.}/testsuites/segmented-diskqueue-crash-driver.sh
