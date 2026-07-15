#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
#
# Copyright 2026 Rainer Gerhards and Adiscon GmbH.
#
# Run the segmented DA disk-space backpressure and reclamation oracle with the
# FixedArray memory tier; the base test retains LinkedList as its default.
export DA_QUEUE_TYPE=FixedArray
. ${srcdir:=.}/segmented-da-maxdiskspace.sh
