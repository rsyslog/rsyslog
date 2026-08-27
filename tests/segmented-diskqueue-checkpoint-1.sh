#!/bin/bash
# Verify checkpointInterval=1 publishes each single-record frontier advance.
export CHECKPOINT_INTERVAL=1
. ${srcdir:=.}/testsuites/segmented-diskqueue-checkpoint-driver.sh
