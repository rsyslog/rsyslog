#!/bin/bash
# RSA key exchange needs no shared group: verify successful TLS 1.2 delivery
# does not generate a misleading normal diagnostic on either endpoint.
# This file is part of the rsyslog project, released under ASL 2.0.
export OSSL_TEST_CIPHER=AES128-GCM-SHA256
. "${srcdir:=.}/sndrcv_tls_ossl_group_diagnostic.sh"
