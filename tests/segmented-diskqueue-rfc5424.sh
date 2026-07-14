#!/bin/bash
# Run the RFC5424 structured-data plus message-JSON queue regression against
# segmentedDisk with the same sequence oracle as the classic pure Disk test.
export QUEUE_TYPE=segmentedDisk
. ${srcdir:=.}/diskq-rfc5424.sh
