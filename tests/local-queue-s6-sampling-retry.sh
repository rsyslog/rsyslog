#!/bin/bash
# Logical sampling keeps only ID2. The existing cooperative FORCE_TERM fixture
# must still replay its accepted action on BE (A=1,2 and B=2), proving neither
# FE transfer nor retry runs the sampler again. Final source totals reconcile.
export LOCAL_QUEUE_S6_RETRY_SAMPLING=1
. ${srcdir:=.}/local-queue-force-term-actions.sh
