#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Valgrind run of imtcp-tls-gtls-certinfo-long-names.sh: checks that the
# GnuTLS peer certificate info dump for long names neither leaks nor makes
# invalid memory accesses.
export USE_VALGRIND="YES"
. ${srcdir:-.}/imtcp-tls-gtls-certinfo-long-names.sh
