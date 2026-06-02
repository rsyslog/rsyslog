#!/usr/bin/env bash
# added 2023-04-20 by Théo Bertin, released under ASL 2.0
## Uncomment for debugging
#export RS_REDIR=-d

export USE_VALGRIND="YES"
. ${srcdir:=.}/omhiredis-template.sh
