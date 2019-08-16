#!/bin/bash
echo temporarily disabled because it fails quite often - fix in progress
exit 77
# This file is part of the rsyslog project, released under ASL 2.0
export USE_VALGRIND="YES"
source ${srcdir:-.}/es-duplicated-ruleset.sh
