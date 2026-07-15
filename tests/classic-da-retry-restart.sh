#!/bin/bash
# Exercise the shared suspended-action restart oracle for both DA memory queue
# types with the classic disk engine explicitly pinned.
for DA_QUEUE_TYPE in LinkedList FixedArray; do
	export DA_QUEUE_TYPE DA_ENGINE=disk
	"${srcdir:=.}/testsuites/da-engine-retry-restart-driver.sh" || exit $?
done
