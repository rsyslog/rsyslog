#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
#
# Copyright 2026 Rainer Gerhards and Adiscon GmbH.
#
# YAML companion of dtls-authmode-unsupported.sh: the same strict / warn /
# backward-compatible policy matrix for an unsupported tls.authmode in the
# omdtls action, the imdtls input and the imdtls module, written through
# the YAML frontend; the oracle is unchanged.
export DTLS_AUTHMODE_YAML=YES
. "${srcdir:=.}/dtls-authmode-unsupported.sh"
