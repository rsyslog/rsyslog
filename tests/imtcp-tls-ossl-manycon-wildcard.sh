#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Concurrent TLS handshakes must share an immutable compiled wildcard list.
# Reuse the complete sequence oracle and TLS setup from the plain-name test.
# Entries are prepended, so the last configured pattern rejects the client
# before the first configured pattern accepts it.
export PERMITTED_PEERS='"*.com","*.invalid"'
. ${srcdir:=.}/imtcp-tls-ossl-manycon.sh
