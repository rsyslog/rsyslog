#!/bin/bash
# YAML action options must reach the same PutRecord backend as RainerScript.
export OMKINESIS_FORMAT=yaml
. ${srcdir:=.}/omkinesis-mock-common.sh
