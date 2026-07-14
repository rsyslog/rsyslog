#!/bin/bash
# Verify checkpointInterval=0 performs no completion-driven state writes.
export CHECKPOINT_INTERVAL=0
export EXPECTED_PERIODIC_WRITES=0
. ${srcdir:=.}/testsuites/segmented-diskqueue-checkpoint-driver.sh
