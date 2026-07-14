#!/bin/bash
# Repeat checkpoint publication crashes twice to bound replay without loss
# across consecutive completion-checkpoint failures.
export SEGDISK_FAULT_POINT=checkpoint-published SEGDISK_FAULT_REPEATS=2
. ${srcdir:=.}/testsuites/segmented-diskqueue-crash-driver.sh
