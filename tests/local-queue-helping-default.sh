#!/bin/bash
# Run the helper priority/conservation fixture with the inherited D=3 cap.
# The final two-message borrow is partial (n<D), without a minimum-batch wait.
export LOCAL_QUEUE_HELP_DEFAULT=1
. ${srcdir:=.}/local-queue-helping.sh
