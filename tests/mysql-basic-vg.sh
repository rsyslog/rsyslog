#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Developed by Adiscon GmbH for the rsyslog testbench.
# Licensed under the Apache License, Version 2.0. See COPYING.ASL20.
#
# Run the basic ommysql storage test under Valgrind. The shared test
# checks the stored sequence after rsyslog finishes writing.
export USE_VALGRIND="YES"
. ${srcdir:=.}/mysql-basic.sh
