#!/bin/bash
# This script "validates" a PR from a git PoV. We check that best
# practices are kept which most importantly means we try to detect
# fixup commits. These should not be part of a PR and squashed into
# the primary commit.
# Copyright (C) 2019 by Rainer Gerhards
exitcode=0;
git log  --oneline HEAD...origin/main >gitlog
if head -1 gitlog | grep -q "Merge " gitlog; then
	printf 'This looks like a github merge branch. Removing first commit:\n'
	head -1 gitlog
	sed -i '1d' gitlog
fi
printf '%d commits in feature branch:\n' $(wc -l < gitlog)
cat gitlog
printf '\n'

if ! tests/CI/check_commit_text.py gitlog; then
	cat >&2 <<- EOF

	There are problems with the commit titles. Please fix them, amend your
	commit via "git commit --amend" and force-push the fixed branch via
	"git push -f".

	EOF
	cat <<- EOF
	For more info, please see
	https://rainer.gerhards.net/2019/03/howto-great-pull-request.html#descriptive_commit_message

	EOF
	exitcode=1
fi

if [ $(wc -l < gitlog) -ge 3 ]; then
	cat >&2 <<- EOF
	This feature branch contains quite some commits. Consider squashing them.

	Note: you can use
	$ git commit -a --amend
	to apply changes to a previous commit. No new commit will be created if
	done so.

	You possibly wonder why this CI check complains. See
	    https://rainer.gerhards.net/2019/03/squash-your-pull-requests.html

	for the reasons and ways to fix the problems. It also describes what
	happens if you do/want/can not fix the problems found by this check.

	EOF
	cat <<- EOF
	For more info, please see
	https://rainer.gerhards.net/2019/03/howto-great-pull-request.html#onecommit_featurepr

	EOF
	exitcode=1
fi
if grep -q "Merge " gitlog; then
	grep "Merge " gitlog
	cat >&2 <<- EOF
	This feature branch contains merge commits. This almost always indicates that it
	contains unwanted merges where a rebase should have been applied.
	Please consider squashing the branch and/or rebasing it to current default branch.

	You possibly wonder why this CI check complains. See
	    https://rainer.gerhards.net/2019/03/squash-your-pull-requests.html

	for the reasons and ways to fix the problems. It also describes what
	happens if you do/want/can not fix the problems found by this check.

	EOF
	exitcode=1
fi

if [ "$exitcode" != "0" ]; then
	cat >&2 <<- EOF
	======================================================================
	General advise on best practices for rsyslog pull requests:
	    https://rainer.gerhards.net/2019/03/howto-great-pull-request.html
	======================================================================
	EOF
fi
exit $exitcode
