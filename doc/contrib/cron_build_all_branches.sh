#!/bin/bash

# Do not allow use of unitilized variables
set -u

# Exit if any statement returns a non-true value
set -e


##############################################################
# Functions
##############################################################

get_latest_stable_branch() {

    git branch -r | \
        grep origin | \
        grep -v "HEAD" | \
        sed -e 's/ //g' -e 's#origin/##g' | \
        grep -E 'v[0-9]+' | \
        tail -n 1
}

# This includes 'main', but intentionally excludes the stable branches
# which main is periodically merged into
get_dev_branches() {

    git branch -r | \
        grep origin | \
        grep -v "HEAD" | \
        sed -e 's/ //g' -e 's#origin/##g' | \
        grep -v "$latest_stable_branch"

}

# The assumption here is that the latest tag == latest stable tag
get_latest_tag() {

    # Treat periods as field separators
    # Sort on second "field"
    # Perform general numerical sort
    # Apply grep filter to make sure we pull in stable tags only (no '-dev'
    #   suffixes, etc)
    # Grab the last tag from the list
    git tag --list 'v*' | \
        sort -t '.' -k2 -g | \
        grep -Eo '^v[.0-9]+$' | \
        tail -n 1

}

# The latest stable tag, but without the leading 'v'
get_latest_stable_version() {

    get_latest_tag | sed "s/[A-Za-z]//g"

}

prep_branch_for_build() {

    branch=$1

    echo "Reset Branch $branch"
    git reset --hard

    echo "Tossing all non-committed changes"
    git clean --force -d

    echo "Checkout Branch $branch"
    git checkout origin/$branch ||
      { echo "[!] Checkout $branch failed... aborting"; exit 1; }

}

update_build_conf_variables() {

    version_string=$1
    release_string=$2
    sphinx_build_conf=$3

    # Replace the entire line in the Sphinx build config file
    sed -r -i "s/^version.*$/version = \'${version_string}\'/" ./${sphinx_build_conf}
    sed -r -i "s/^release.*$/release = \'${release_string}\'/" ./${sphinx_build_conf}

}



##############################################################
# Offer opportunity to stop before (destructive) work begins
##############################################################

echo ""
echo "This script is intended to run with a clean repo version of the code."
echo "Run the sphinx-build command manually if you want to see your uncommited changes."
echo "If you run this script with uncommitted and un-pushed changes, YOU WILL LOSE THOSE CHANGES!"
echo ""
echo "Press Enter to continue or Ctrl-C to cancel...."
read -r REPLY


# Refresh local content
echo "Fetching latest changes from origin ..."
git fetch origin --prune --tags



##############################################################
# Setup
##############################################################

# Set to newlines only so spaces won't trigger a new array entry and so loops
# will only consider items separated by newlines to be the next in the loop
IFS=$'\n'

# Formats that are built by default for new releases
declare -a formats
formats=(
  epub
  html
)

latest_stable_branch=$(get_latest_stable_branch)
dev_branches=($(get_dev_branches))

# Build release docs for latest official stable version
latest_stable_tag=$(get_latest_tag)

# The latest stable tag, but without the leading 'v'
latest_stable_version=$(get_latest_stable_version)

# tarball representing the documentation for the latest stable release
doc_tarball="rsyslog-doc-${latest_stable_version}.tar.gz"


# Allow user to pass in the location for generated files
# If user opted to not pass in the location, go ahead and set a default.
if [[ -z ${1+x} ]]; then
    output_dir="/tmp/rsyslog-doc-builds"
    echo -e "\nWARNING: Output directory not specified, falling back to default: $output_dir"
else
    # Otherwise, if they DID pass in a value, use that.
    output_dir=$1
    echo "Generated files will be placed in ${output_dir}"
fi

# The build conf used to generate release output files. Included
# in the release tarball and needs to function as-is outside
# of a Git repo (e.g., no ".git" directory present).
sphinx_build_conf_prod="source/conf.py"



###############################################################
# Prep work
###############################################################

mkdir -p ${output_dir}



###############################################################
# Build formats for each dev branch
##############################################################

# Prep each branch for fresh build
for branch in "${dev_branches[@]}"
do
    prep_branch_for_build $branch

    for format in "${formats[@]}"
    do
        echo "Building $format for $branch branch"

        # Use values calculated by conf.py (geared towards dev builds) instead
        # of overriding them.
        sphinx-build -b $format source $output_dir/$branch ||
          { echo "[!] sphinx-build $format failed for $branch branch ... aborting"; exit 1; }

    done
done


###############################################################
# Build formats for stable branch
###############################################################

prep_branch_for_build $latest_stable_branch

# Reduce X.Y.Z to just X.Y
latest_stable_major_minor_version=$(basename $latest_stable_version ".0")

# Replace the existing hard-coded placeholder values with current
# info sourced from the Git repo.
update_build_conf_variables \
    ${latest_stable_major_minor_version} \
    "${latest_stable_version}-stable" \
    ${sphinx_build_conf_prod}

for format in "${formats[@]}"
do
    echo "Building $format for $latest_stable_branch branch"

    sphinx-build -b $format source $output_dir/$latest_stable_branch ||
      { echo "[!] sphinx-build $format failed for $latest_stable_branch branch ... aborting"; exit 1; }
done


###############################################################
# Build latest stable tag (HTML only)
###############################################################

echo "Building latest stable tag: $latest_stable_tag"
git reset --hard
git clean --force -d
git checkout $latest_stable_tag

# This modified source/conf.py build conf is included in the
# tarball for later use by downstream package maintainers
# or anyone else that wishes to build from source using
# only the tarball (e.g., no .git directory or repo present)
update_build_conf_variables \
    ${latest_stable_major_minor_version} \
    $latest_stable_version \
    ${sphinx_build_conf_prod}

sphinx-build -b html source build ||
    { echo "[!] sphinx-build failed for html format of $latest_stable_tag tag ... aborting"; exit 1; }

tar -czf $output_dir/$doc_tarball source build LICENSE README.md ||
    { echo "[!] tarball creation failed for $latest_stable_tag tag ... aborting"; exit 1; }


###############################################################
# Reset repo to main for next build
###############################################################

# This ensures that the build script is always at the latest version
prep_branch_for_build main
