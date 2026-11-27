#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Test for queue data persisting at shutdown. We use the actual driver
# to carry out multiple tests with different queue modes
# added 2009-05-27 by Rgerhards
# Developed by Adiscon GmbH for the rsyslog testbench.
# Licensed under the Apache License, Version 2.0. See COPYING.ASL20.
#
# The disabled test currently exits with a skip. Its driver exercises
# disk-assisted persistence across shutdown for both linked-list and
# fixed-array queues.
echo ===============================================================================
echo \[daqueue-persist.sh\]: test data persisting at shutdown
echo TEST is currently DISABLE because it is unstable
exit 77
echo mode linkedList
$srcdir/daqueue-persist-drvr.sh LinkedList
echo mode fixedArray
$srcdir/daqueue-persist-drvr.sh FixedArray
# the disk test should not fail, however, the config is extreme and using
# it more or less is a config error
echo Disk
$srcdir/daqueue-persist-drvr.sh Disk
# we do not test Direct mode because this absolute can not work in direct mode
# (maybe we should do a fail-type of test?)
