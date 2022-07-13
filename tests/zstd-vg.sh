#!/bin/bash
export USE_VALGRIND="YES"
export NUMMESSAGES=10000 # valgrind is pretty slow, so we need to user lower nbr of msgs
source ${srcdir:-.}/zstd.sh
