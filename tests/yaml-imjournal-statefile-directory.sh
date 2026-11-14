#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# YAML companion of imjournal-statefile-directory.sh: the imjournal module
# with statefile and ratelimit.interval is loaded from an included YAML
# file (the filter stays in RainerScript); the state file path is a
# directory. Scenario and oracle (message written exactly once, path still
# a directory after shutdown) are documented in
# imjournal-statefile-directory.sh.
export IMJOURNAL_TEST_YAML=1
. ${srcdir:=.}/imjournal-statefile-directory.sh
