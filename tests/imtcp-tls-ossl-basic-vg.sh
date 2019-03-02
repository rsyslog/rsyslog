#!/bin/bash
if [ "$(valgrind --version)" == "valgrind-3.11.0" ]; then
	printf 'This test does NOT work with valgrind-3.11.0 - valgrind always reports\n'
	printf 'a valgrind-internal bug. So we need to skip it.\n'
	exit 77
fi
export USE_VALGRIND="YES"
source ${srcdir:-.}/imtcp-tls-ossl-basic.sh
