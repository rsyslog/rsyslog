#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Exercise the same legacy-compression diagnostics through native YAML actions.
export LEGACY_WARNING_YAML=1
. "${srcdir:=.}/omfwd-legacy-compression-warning.sh"
