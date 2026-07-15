#!/bin/bash
# Run the shared save-on-shutdown/restart oracle for both DA memory queue types
# with automatic segmented storage.
for DA_QUEUE_TYPE in LinkedList FixedArray; do
	export DA_QUEUE_TYPE DA_ENGINE=auto
	"${srcdir:=.}/testsuites/da-engine-persist-driver.sh" || exit $?
done
