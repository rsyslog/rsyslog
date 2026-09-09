#!/bin/bash
# Valgrind wrapper for queue-linkedlist-enq-prealloc.sh. Oracle is the same
# ordered delivery check; Valgrind must report no leaks from unused
# preallocated LinkedList nodes on the qqueueEnqMsg() path.
export USE_VALGRIND="YES"
export NUMMESSAGES="${NUMMESSAGES:-200}"
. ${srcdir:-.}/queue-linkedlist-enq-prealloc.sh
