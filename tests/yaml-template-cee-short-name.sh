#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
#
# Copyright 2026 Rainer Gerhards and Adiscon GmbH.
#
# YAML companion of template-cee-short-name.sh: the same short CEE /
# local-variable template is defined through the YAML frontend; the jsonf
# key and bare-indicator oracle is unchanged.
export CEE_SHORT_YAML=YES
. "${srcdir:=.}/template-cee-short-name.sh"
