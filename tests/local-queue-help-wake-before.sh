#!/bin/bash
# Inject BE work at the test-only before-registration gate. Admission,
# FIFO release and callback markers prove the real idle handoff cannot lose
# this wake; timeout is a watchdog, never evidence of success.
export LOCAL_QUEUE_HELP_GATE=before
. ${srcdir:=.}/local-queue-helping.sh
