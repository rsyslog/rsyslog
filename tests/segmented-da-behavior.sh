#!/bin/bash
# Run the shared LinkedList/FixedArray and main/ruleset/action spill oracle with
# the automatic default, which must resolve fresh queues to segmented disk.
for DA_QUEUE_TYPE in LinkedList FixedArray; do
	for DA_SCOPE in main ruleset action; do
		export DA_SCOPE DA_QUEUE_TYPE DA_ENGINE=auto
		"${srcdir:=.}/testsuites/da-engine-behavior-driver.sh" || exit $?
	done
done
