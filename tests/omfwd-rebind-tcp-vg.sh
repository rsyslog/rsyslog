#!/bin/bash
# Valgrind wrapper for omfwd-rebind-tcp.sh.  Keep the scenario in the base
# test so the normal and Valgrind registrations exercise the same logic.
export USE_VALGRIND="YES"
. ${srcdir:=.}/omfwd-rebind-tcp.sh
