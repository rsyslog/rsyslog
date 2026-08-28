#!/bin/bash
# Run the JSON-variable expression semantics regression under Valgrind.
# This file is part of the rsyslog project, released under ASL 2.0.
export USE_VALGRIND="YES"
. ${srcdir:-.}/rscript-json-var-eval-semantics.sh
