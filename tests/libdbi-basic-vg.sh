#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Developed by Adiscon GmbH for the rsyslog testbench.
# Licensed under the Apache License, Version 2.0. See COPYING.ASL20.
#
# Run the basic libdbi-to-MySQL storage test under Valgrind. The shared
# test checks the sequence read back from the database after shutdown.
# this test is currently not included in the testbench as libdbi
# itself seems to have a memory leak
export USE_VALGRIND="YES"
. ${srcdir:=.}/libdbi-basic.sh
