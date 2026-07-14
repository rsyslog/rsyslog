#!/bin/bash
# Run the complete-event persistence oracle through a FixedArray segmented DA
# child, complementing the LinkedList wrapper with identical typed JSON,
# message-variable, local-variable, and RFC5424 field comparisons.
export SEGDISK_EVENT_DA=1
export SEGDISK_DA_QUEUE_TYPE=FixedArray
. ${srcdir:=.}/segmented-diskqueue-event-properties.sh
