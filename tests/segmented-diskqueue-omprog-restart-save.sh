#!/bin/bash
# Run the pure-disk negative-ack/restart regression unchanged against the
# segmentedDisk backend, proving retry-before-commit parity.
export QUEUE_TYPE=segmentedDisk
. ${srcdir:=.}/omprog-diskq-restart-save.sh
