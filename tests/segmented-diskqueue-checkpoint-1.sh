#!/bin/bash
# Verify checkpointInterval=1 publishes each single-record frontier advance.
export CHECKPOINT_INTERVAL=1
export EXPECTED_PERIODIC_WRITES=12
. ${srcdir:=.}/testsuites/segmented-diskqueue-checkpoint-driver.sh
