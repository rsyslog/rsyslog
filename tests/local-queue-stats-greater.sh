#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Check literal and counter-to-counter thresholds in the local-queue oracle.
# A date token equal to a literal threshold must not erase the rhs operand.
# Prewritten records make every successful check immediate; the normal harness
# timeout remains hang protection for a broken parser, not a timing oracle.
. ${srcdir:=.}/diag.sh init
. "$srcdir/local-queue-common.sh"
STATSFILE="$PWD/${RSYSLOG_DYNNAME}.stats"
for day in 5 6 7; do
    printf 'Fri Nov %s 15:10:25 2026: main Q.local: origin=core.queue.local count=7 other=3\n' "$day" > "$STATSFILE"
    localq_wait_stats_greater "$STATSFILE" "main Q.local" count 6
    localq_wait_stats_greater "$STATSFILE" "main Q.local" count other
done
printf 'Fri Nov 6 15:10:25 2026: main Q.local: origin=core.queue.local count=2027\n' > "$STATSFILE"
localq_wait_stats_greater "$STATSFILE" "main Q.local" count 2026
exit_test
