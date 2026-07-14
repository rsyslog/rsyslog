#!/bin/bash
# Run the shared DA main/ruleset/action spill oracle with the automatic default,
# which must resolve a fresh queue to the segmented disk engine.
for DA_SCOPE in main ruleset action; do
	export DA_SCOPE DA_ENGINE=auto
	"${srcdir:=.}/testsuites/da-engine-behavior-driver.sh" || exit $?
done
