#!/bin/bash
# added 2020-01-17 by RGerhards, released under ASL 2.0
export RS_TLS_DRIVER=mbedtls
export TEXT_TO_CHECK="X509 - Certificate verification failed"
#export RSYSLOG_DEBUG="debug nologfuncflow noprintmutexaction nostdout"
#export RSYSLOG_DEBUGLOG="log"
source ${srcdir:=.}/sndrcv_tls_certvalid_revoked.sh
