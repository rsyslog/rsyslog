#!/bin/bash
# Exercise side-effect-free verification and fatal local omfile startup checks
# through native YAML queue/action objects using the common process-exit oracle.
export LOCAL_OMFILE_ACTIVATION_YAML=1
. ${srcdir:=.}/local-queue-omfile-activation.sh
