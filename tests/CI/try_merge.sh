#!/bin/bash
oldbranch=$(git rev-parse --abbrev-ref HEAD)
# note: we usually do not have permissons to modify git config --global,
# so we do it just to the local context, which is fine with us.
git config user.email "buildbot@rsyslog.com"
git config user.name "buildbot"

# we need to make sure we can pull in new code from the main
# repo. Travis, for example, checks us out into a different repo.
git remote add github https://github.com/rsyslog/rsyslog.git
git fetch github &> /dev/null

# create our interim branch and go...
git branch -D tmp-CI
git branch tmp-CI
git checkout tmp-CI
git merge --no-edit github/master
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

if [ "$1" != "--merge-only" ]; then
    # from here on, we want to see what's going on and we also want to abort
    # on any error.
    set -v
    set -e
    tests/CI/clang-build.sh
    git checkout ${oldbranch}
fi
