#!/bin/bash
# Run the segmented DA disk-space backpressure and reclamation oracle with the
# FixedArray memory tier; the base test retains LinkedList as its default.
export DA_QUEUE_TYPE=FixedArray
. ${srcdir:=.}/segmented-da-maxdiskspace.sh
