#!/bin/bash
# test for b64_decode function in rainerscript
# added 2024-06-11 by KGuillemot
# This file is part of the rsyslog project, released under ASL 2.0
export USE_VALGRIND="YES"
source ${srcdir:-.}/rscript_b64_decode.sh
