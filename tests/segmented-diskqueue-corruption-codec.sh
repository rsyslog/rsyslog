#!/bin/bash
# Run the segmentedDisk corruption driver with valid payload CRC but invalid TLV.
export CORRUPTION_KIND=codec
. ${srcdir:=.}/segmented-diskqueue-corruption.sh
