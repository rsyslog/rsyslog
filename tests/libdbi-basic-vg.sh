#!/bin/bash
# This file is part of the rsyslog project, released under GPLv3
# this test is currently not included in the testbench as libdbi
# itself seems to have a memory leak
export USE_VALGRIND="YES"
source ${srcdir:=.}/libdbi-basic.sh
