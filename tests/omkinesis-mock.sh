#!/bin/bash
# Copyright 2026 Adiscon GmbH.
# This file is part of the rsyslog project, released under ASL 2.0.
# RainerScript action options must produce a signed, correctly encoded PutRecord.
export OMKINESIS_FORMAT=rainerscript
. ${srcdir:=.}/omkinesis-mock-common.sh
