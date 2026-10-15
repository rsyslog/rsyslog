#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# RainerScript entry point for the persistent-connection named-policy lifecycle oracle.
export RSYSLOG_NAMED_POLICY_FORMAT=rs
. ${srcdir:=.}/testsuites/imtcp-named-policy-driver.sh
