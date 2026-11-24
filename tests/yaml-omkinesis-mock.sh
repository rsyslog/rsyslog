#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 Adiscon GmbH.
# This file is part of the rsyslog project, released under ASL 2.0.
# YAML action options must reach the same PutRecord backend as RainerScript.
export OMKINESIS_FORMAT=yaml
. ${srcdir:=.}/omkinesis-mock-common.sh
