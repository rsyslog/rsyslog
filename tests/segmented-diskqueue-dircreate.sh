#!/bin/bash
# Run automatic dynamic-file directory creation with segmentedDisk.
export QUEUE_TYPE=segmentedDisk
. ${srcdir:=.}/dircreate_dflt.sh
