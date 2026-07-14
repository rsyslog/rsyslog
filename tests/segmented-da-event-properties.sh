#!/bin/bash
# Run the complete-event persistence oracle through a segmented DA child.
export SEGDISK_EVENT_DA=1
. ${srcdir:=.}/segmented-diskqueue-event-properties.sh
