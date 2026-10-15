#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Native YAML entry point for the same named-policy lifecycle and sharing oracle.
export RSYSLOG_NAMED_POLICY_FORMAT=yaml
. ${srcdir:=.}/testsuites/imtcp-named-policy-driver.sh
