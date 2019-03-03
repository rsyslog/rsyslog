#!/bin/bash
# This script "validates" a PR from a git PoV. We check that best
# practices are kept which most importantly means we try to detect
# fixup commits. These should not be part of a PR and squashed into
# the main commit.
# Copyright (C) 2019 by Rainer Gerhards
exitcode=0;
git log  --oneline HEAD...origin/master >gitlog
if head -1 gitlog | grep -q "Merge " gitlog; then
	printf 'This looks like a github merge branch. Removing first commit:\n'
	head -1 gitlog
	sed -i '1d' gitlog
fi
printf '%d commits in feature branch:\n' $(wc -l < gitlog)
cat gitlog
printf '\n'
if [ $(wc -l < gitlog) -ge 5 ]; then
	cat <<- EOF
	This feature branch contains quite some commits. Consider squashing it.

	EOF
	exitcode=1
fi
if grep "Merge from" gitlog; then
	cat <<- EOF
	This feature branch contains merge commits. This almost always indicates that it
	contains unwanted merges where a rebase should have been applied.
	Please consider squashing the branch and/or rebasing it to current master.

	EOF
	exitcode=1
fi
exit $exitcode
