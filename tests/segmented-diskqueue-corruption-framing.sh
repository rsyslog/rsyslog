#!/bin/bash
# Run the segmentedDisk corruption driver with record-magic damage.
export CORRUPTION_KIND=framing
. ${srcdir:=.}/segmented-diskqueue-corruption.sh
