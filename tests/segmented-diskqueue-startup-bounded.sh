#!/bin/bash
# Run restart recovery with impstats assertions proving startup reads no payload
# and probes only the bounded current/reserved segment paths.
# This file is part of the rsyslog project, released under ASL 2.0.
export CHECK_STARTUP_STATS=on
. ${srcdir:=.}/segmented-diskqueue-restart.sh
