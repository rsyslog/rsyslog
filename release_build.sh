#!/bin/sh

# this is used to create an rsyslog release tarball.
# it must be executed from the root of the rsyslog-doc
# project.

docfile=rsyslog-doc.tar.gz

[ ! -d ./build ] || rm -rf ./build

sphinx-build -b html source build || {
	echo "sphinx-build failed... aborting"
	exit 1
}

[ ! -e ./$docfile ] || rm -rf ./$docfile

tar -czf $docfile build source LICENSE README.md build.sh || {
	echo "Failed to create rsyslog-doc.tar.gz tarball..."
	exit 1
}
