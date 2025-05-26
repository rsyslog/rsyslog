#!/bin/bash
export USE_VALGRIND="YES"
export NUMMESSAGES=200000 # reduce for slower valgrind run
source ${srcdir:-.}/gzipwr_hup.sh
