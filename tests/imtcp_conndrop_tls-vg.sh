#!/bin/bash
# This file is part of the rsyslog project, released under ASL 2.0
export USE_VALGRIND="YES"
export NUMMESSAGES=10000 # reduce for slower valgrind run
source ${srcdir:-.}/imtcp_conndrop_tls.sh
