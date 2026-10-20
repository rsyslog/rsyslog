#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
export USE_VALGRIND="YES"
export NUMMESSAGES=10000 # valgrind is pretty slow, so we need to user lower nbr of msgs
. ${srcdir:-.}/zstd.sh
