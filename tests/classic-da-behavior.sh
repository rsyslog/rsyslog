#!/bin/bash
# Run the shared DA main/ruleset/action spill oracle with the classic engine
# explicitly pinned, proving that compatibility mode remains available.
for DA_SCOPE in main ruleset action; do
	export DA_SCOPE DA_ENGINE=disk
	"${srcdir:=.}/testsuites/da-engine-behavior-driver.sh" || exit $?
done
