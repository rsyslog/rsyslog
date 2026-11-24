#!/bin/bash
# Exercise the producer-cache allocation failure variant of the shared actual
# imtcp registration-fallback and exact-delivery oracle.
export LOCALQ_TEST_FAULT=producer-cache
. ${srcdir:=.}/local-queue-imtcp-tls-failure.sh
