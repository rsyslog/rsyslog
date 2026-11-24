#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# The imjournal state file path is a FIFO without a writer. imjournal must
# not block opening it, must ignore it, read the journal and must not
# replace the FIFO when saving its position. Scenario and oracle (message
# written exactly once, path still a FIFO after shutdown) are documented
# in imjournal-statefile-directory.sh.
export IMJOURNAL_STATEFILE_KIND=fifo
. ${srcdir:=.}/imjournal-statefile-directory.sh
