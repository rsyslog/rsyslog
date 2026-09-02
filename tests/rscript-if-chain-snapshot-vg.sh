#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
#
# Copyright 2026 Rainer Gerhards and Adiscon GmbH.
#
# Run the selector snapshot regression under Valgrind.
# This file is part of the rsyslog project, released under ASL 2.0.
export USE_VALGRIND="YES"
. ${srcdir:=.}/rscript-if-chain-snapshot.sh
