#!/bin/bash
# Crash after sealing rename; restart reconciles the reserved/current paths.
export SEGDISK_FAULT_POINT=seal-renamed
. ${srcdir:=.}/testsuites/segmented-diskqueue-crash-driver.sh
