#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# The force-termination fixture holds one BE transaction until shutdown. This
# variant disables internal-message processing and configures operatingStateFile
# so the graph's one-time report must state the exact local loss count (one) and
# zero generic memory-queue residuals. The single matching OSF entry and absent
# action-output copy prove the report is visible without re-enqueueing itself.
export LOCAL_QUEUE_REPORT_SHUTDOWN_LOSS=1
. ${srcdir:=.}/local-queue-be-force-term-actions.sh
