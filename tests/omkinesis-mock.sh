#!/bin/bash
# RainerScript action options must produce a signed, correctly encoded PutRecord.
export OMKINESIS_FORMAT=rainerscript
. ${srcdir:=.}/omkinesis-mock-common.sh
