#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# This file is part of the rsyslog project, released under ASL 2.0
# omfwd must reconnect when the TLS peer closed the connection with
# close_notify; see omfwd-tls-close-notify-common.sh for details.
export TLS_DRIVER=ossl
. ${srcdir:=.}/omfwd-tls-close-notify-common.sh
