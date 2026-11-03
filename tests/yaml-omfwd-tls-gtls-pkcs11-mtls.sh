#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
export GTLS_PKCS11_CASE="mtls"
export GTLS_PKCS11_CONF="yaml"
. ${srcdir:-.}/omfwd-tls-gtls-pkcs11-common.sh
