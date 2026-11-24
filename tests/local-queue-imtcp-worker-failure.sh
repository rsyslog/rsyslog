#!/bin/bash
# Exercise FE worker start failure: reserve one descriptor without publishing a
# message, retain its failed generation, and route every actual imtcp batch to BE.
export LOCALQ_TEST_FAULT=frontend-worker
. ${srcdir:=.}/local-queue-imtcp-tls-failure.sh
