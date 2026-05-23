#!/bin/bash
# Valgrind wrapper for mmjsonparse-find-json-conflict.sh. The base test creates
# a msgAddJSON failure after ownership transfer; Valgrind is the oracle that the
# parsed JSON object is released exactly once on that negative path.
# This file is part of the rsyslog project, released under ASL 2.0
export USE_VALGRIND="YES"
srcdir="${srcdir:-$(dirname "$0")}"
. "$srcdir/mmjsonparse-find-json-conflict.sh"
