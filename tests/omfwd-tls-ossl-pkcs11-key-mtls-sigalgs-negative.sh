#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
export PKCS11_SIGALG_VARIANT=key_only
. ${srcdir:=.}/omfwd-tls-ossl-pkcs11-sigalgs-negative-common.sh
