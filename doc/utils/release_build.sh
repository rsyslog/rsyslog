#!/bin/bash

# Do not allow use of uninitialized variables
set -u

# Exit if any statement returns a non-true value
set -e


# Read the riot act, give user an option to bail before building the docs

echo <<DISCLAIMER "

RELEASE: $1 (do NOT put a V in front of the plain version)

#######################################################################
#                                                                     #
# Purpose: Create an official rsyslog docs release tarball            #
#                                                                     #
# Before proceeding, please confirm that you have performed the       #
# following steps:                                                    #
#                                                                     #
# 1. Manually fetched, merged and tagged the changes to the stable    #
#    branch that are intended to reflect the latest release.          #
#                                                                     #
# 2. Checkout the latest tag (or stable branch)                       #
#                                                                     #
# 3. Remove uncommitted files to help prevent them from being         #
#    included in the release tarball                                  #
#                                                                     #
# These steps can be automated, but have been left as-is for this     #
# version of the release script. If desired, a future version of      #
# the script can be enhanced to include this functionality.           #
#                                                                     #
#                                                                     #
#             PRESS ENTER TO CONTINUE OR CTRL+C TO CANCEL             #
#                                                                     #
#######################################################################
"
DISCLAIMER

read -r REPLY



#####################################################################
# Setup
#####################################################################

# The latest stable tag, but without the leading 'v'
# Format: X.Y.Z
release=$1

# The release version, but without the trailing '.0'
# Format: X.Y
version=$(echo $release | sed 's/\.0//')

# Use the full version number
docfile=rsyslog-doc-${release}.tar.gz

# Hard-code html format for now since that is the only format
# officially provided
format="html"

# The build conf used to generate release output files. Included
# in the release tarball and needs to function as-is outside
# of a Git repo (e.g., no ".git" directory present).
sphinx_build_conf_prod="source/conf.py"


#####################################################################
# Sanity checks
#####################################################################

[ -d ./.git ] || {
    echo "[!] Pre-build check fail: .git directory missing"
    echo "    Run $0 again from a clone of the rsyslog-doc repo."
    exit 1
}


#####################################################################
# Fill out template
#####################################################################

# To support building from sources included in the tarball AND
# to allow dynamic version generation we modify the placeholder values
# in the Sphinx build configuration file to reflect the latest stable
# version. Without this (e.g., if someone just calls sphinx-build
# directly), then dev build values will be automatically calculated
# using info from the Git repo if available, or will fallback to
# the placeholder values provided for non-Git builds (e.g., someone
# downloads the 'master' branch as a zip file from GitHub).

sed -r -i "s/^version.*$/version = \'${version}\'/" ./${sphinx_build_conf_prod} || {
    echo "[!] Failed to replace version placeholder in Sphinx build conf template."
    exit 1
}

sed -r -i "s/^release.*$/release = \'${release}\'/" ./${sphinx_build_conf_prod} || {
    echo "[!] Failed to replace release placeholder in Sphinx build conf template."
    exit 1
}


#####################################################################
# Cleanup from last run
#####################################################################

[ ! -d ./build ] || rm -rf ./build || {
    echo "[!] Failed to prune old build directory."
    exit 1
}

[ ! -e ./$docfile ] || rm -f ./$docfile || {
    echo "[!] Failed to prune $docfile tarball"
    exit 1
}


#####################################################################
# Build HTML format
#####################################################################

sphinx-build -b $format source build || {
	echo "sphinx-build failed... aborting"
	exit 1
}


#####################################################################
# remove caches and other unwanted content
#####################################################################
rm -rf source/_ext/__pycache__
rm -rf source/__pycache__/

#####################################################################
# Create dist tarball
#####################################################################

tar -czf $docfile build source LICENSE README.md || {
	echo "Failed to create $docfile tarball..."
	exit 1
}

echo creating password-protected zip for email use
zip -e $docfile.zip $docfile
echo remember to git add the updated files, and tag once everything is OK
