#!/bin/sh

# this is used to create an rsyslog release tarball.
# it must be executed from the root of the rsyslog-doc
# project.


if [ -z $1 ]; then
    echo "[!] Release version number missing. Please run $0 again"
    echo "    with the stable x.y release number that reflects HEAD"
    exit 1
else
    version="$1"
fi

release="${version}.0"

docfile=rsyslog-doc-$1.tar.gz

# Hard-code html format for now since that is the only format
# officially provided
format="html"

[ ! -d ./build ] || rm -rf ./build


sphinx-build -D version="$version" -D release="$release" -b $format source build || {
	echo "sphinx-build failed... aborting"
	exit 1
}

[ ! -e ./$docfile ] || rm -rf ./$docfile

tar -czf $docfile build source LICENSE README.md || {
	echo "Failed to create $docfile tarball..."
	exit 1
}
