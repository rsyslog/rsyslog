#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Run the segmentedDisk corruption driver with valid payload CRC but invalid TLV.
export CORRUPTION_KIND=codec
. ${srcdir:=.}/segmented-diskqueue-corruption.sh
