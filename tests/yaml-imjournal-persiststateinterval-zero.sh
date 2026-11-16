#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# YAML companion of imjournal-persiststateinterval-zero.sh: configures imjournal
# with a state file and persiststateinterval "0" through a YAML-only config.
# Oracle (shared scenario): the injected message is received exactly once,
# rsyslogd shuts down cleanly and the state file is written at shutdown.
# Skips (77) under the same journal-availability conditions as the base test.
export IMJOURNAL_PSI0_YAML=1
. ${srcdir:=.}/imjournal-persiststateinterval-zero.sh
