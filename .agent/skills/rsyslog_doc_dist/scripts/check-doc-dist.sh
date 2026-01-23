#!/bin/bash
# .agent/skills/rsyslog_doc_dist/scripts/check-doc-dist.sh
# Extended verification for documentation distribution.
# This script ensures that any added/deleted doc files are correctly
# registered in Makefile.am and present in the distribution tarball.

set -e

REPO_ROOT=$(git rev-parse --show-toplevel)
cd "$REPO_ROOT"

echo ">>> Starting extended doc distribution check..."

# 1. Ensure a clean state and build
echo ">>> Running regular build and doc validation..."
make -j$(nproc) check TESTS=""
make -C doc html

# 2. Get version from configure.ac
VERSION=$(grep "AC_INIT" configure.ac | cut -d, -f2 | tr -d '[] ')
echo ">>> Detected rsyslog version: $VERSION"

# 3. Create distribution tarball
echo ">>> Creating distribution tarball..."
make dist

TARBALL="rsyslog-$VERSION.tar.gz"
if [ ! -f "$TARBALL" ]; then
    echo ">>> ERROR: Tarball $TARBALL not found!"
    exit 1
fi

# 4. Unpack and verify
DIST_DIR="dist_verify_$$"
mkdir "$DIST_DIR"
echo ">>> Unpacking into $DIST_DIR..."
tar -xzf "$TARBALL" -C "$DIST_DIR"
cd "$DIST_DIR/rsyslog-$VERSION"

# 5. Run the "Special Case" build check
echo ">>> Running extended verification (autoreconf + configure.sh)..."
# Note: The user specified ./configure.sh as a special case. 
# In this environment, we check if it exists, otherwise we use ./configure.
CONFIGURE_SCRIPT="./configure.sh"
if [ ! -f "$CONFIGURE_SCRIPT" ]; then
    echo ">>> WARNING: $CONFIGURE_SCRIPT not found. Falling back to ./configure (adjust if needed)."
    CONFIGURE_SCRIPT="./configure"
fi

autoreconf -fvi
$CONFIGURE_SCRIPT --enable-debug --enable-testbench

# 6. Verify documentation build in the distribution
echo ">>> Verifying doc build in the distribution..."
# Entering doc directory in the unpacked source
cd doc
make html

echo ">>> SUCCESS: Documentation distribution check passed."

# 7. Cleanup
cd "$REPO_ROOT"
rm -rf "$DIST_DIR"
rm "$TARBALL"
