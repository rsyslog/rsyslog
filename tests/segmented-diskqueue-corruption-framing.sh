#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Run the segmentedDisk corruption driver with record-magic damage.
export CORRUPTION_KIND=framing
. ${srcdir:=.}/segmented-diskqueue-corruption.sh
