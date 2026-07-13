#!/bin/bash
# Verify checkpointInterval=5 counts records and publishes after each five.
export CHECKPOINT_INTERVAL=5
export EXPECTED_PERIODIC_WRITES=2
. ${srcdir:=.}/testsuites/segmented-diskqueue-checkpoint-driver.sh
