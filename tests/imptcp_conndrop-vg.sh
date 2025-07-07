#!/bin/bash
export USE_VALGRIND="YES"
export TB_TEST_MAX_RUNTIME=1200 # connection drops are very slow...
export NUMMESSAGES=10000 # even if it is slow, we use a large number to be
	# sure to have sufficient connection drops - but as low as possible!
source ${srcdir:-.}/imptcp_conndrop.sh
