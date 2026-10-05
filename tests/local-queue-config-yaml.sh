#!/bin/bash
# Run the same local-queue acceptance/rejection cases through the YAML frontend.
# The shared script documents the process-status/diagnostic oracle and hang guard.
export LOCAL_QUEUE_CONFIG_YAML=1
. ${srcdir:=.}/local-queue-config.sh
