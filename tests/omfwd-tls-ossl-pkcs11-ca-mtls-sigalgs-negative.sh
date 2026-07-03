#!/bin/bash
export PKCS11_SIGALG_VARIANT=ca_key
. ${srcdir:=.}/omfwd-tls-ossl-pkcs11-sigalgs-negative-common.sh
