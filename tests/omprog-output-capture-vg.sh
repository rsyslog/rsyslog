#!/bin/bash
# This file is part of the rsyslog project, released under ASL 2.0
export USE_VALGRIND="YES"
export RS_TEST_VALGRIND_EXTRA_OPTS="--child-silent-after-fork=yes"
source ${srcdir:-.}/omprog-output-capture.sh
