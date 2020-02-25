#!/bin/bash
export USE_VALGRIND="YES"
export NUMMESSAGES=200000 # reduce for slower valgrind run
source ${srcdir:-.}/omfile_hup.sh
