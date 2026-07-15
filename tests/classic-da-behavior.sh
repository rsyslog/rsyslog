#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
#
# Copyright 2026 Rainer Gerhards and Adiscon GmbH.
#
# Run the shared LinkedList/FixedArray and main/ruleset/action spill oracle with
# classic disk explicitly pinned, proving compatibility mode remains available.
# This matrix wrapper deliberately leaves diag.sh initialization to each driver
# invocation; sourcing it here would initialize one shared test instance six
# times instead of giving every scenario its own harness lifecycle.
for DA_QUEUE_TYPE in LinkedList FixedArray; do
	for DA_SCOPE in main ruleset action; do
		export DA_SCOPE DA_QUEUE_TYPE DA_ENGINE=disk
		"${srcdir:=.}/testsuites/da-engine-behavior-driver.sh" || exit $?
	done
done
