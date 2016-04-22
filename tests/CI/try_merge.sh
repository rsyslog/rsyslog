#!/bin/bash
oldbranch=`git rev-parse --abbrev-ref HEAD`
git config --global user.email "buildbot@rsyslog.com"
git config --global user.name "buildbot"
git branch -D tmp-CI
git branch tmp-CI
git checkout tmp-CI
git fetch origin
git merge --no-edit origin/master
if [ $? -ne 0 ]; then
    echo "======================================================================"
    echo "= FAIL: git merge, not doing any tests on the merge result           ="
    echo "======================================================================"
    echo "Note: this is not an error per se, can happen. In this case the user"
    echo "must be somewhat more careful when merging."
    git merge --abort
    git checkout ${oldbranch}
    exit 0
fi
echo "======================================================================"
echo "= SUCCESS: git merge; doing some more tests on master+PR             ="
echo "======================================================================"
echo "Note: failing tests may not be reproducible if master branch advances."
echo "      However, they should not be taken lightly as they point into to"
echo "      two conflicting changes to the codebase."

# from here on, we want to see what's going on and we also want to abort
# on any error.
set -v
set -e

tests/CI/clang-check-sanitizer.sh

git checkout ${oldbranch}
