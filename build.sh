#!/bin/sh

echo "This script is intended to run with the clean repo version of the code."
echo "Run the sphinx-build command manually if you want to see your uncommited changes."
echo "If you run this command with uncommitted and un-pushed changes, you will lose those changes,"
echo "or end up with merge conflicts."
echo ""
echo "Press Enter to continue or Ctrl-C to cancel...."
read

MASTERBRANCH=v8-devel

for version in $(git branch | cut -c3-)
  do
    ver=$MASTERBRANCH
    [ "$version" = master ] || ver=$version
    echo "Checkout Branch $version"
    git checkout $version
    echo "Fetch Branch $version"
    git fetch origin $version
    echo "Reset Branch $version"
    git reset --hard
    echo "Pull Branch $version"
    git pull origin $version
    echo "Build $version"
    sphinx-build -b html source $ver
done
