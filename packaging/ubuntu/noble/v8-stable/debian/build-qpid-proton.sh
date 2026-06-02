#!/bin/bash
set -e

# Use DESTDIR from environment or default to debian/tmp
DESTDIR="${DESTDIR:-$(pwd)/debian/tmp}"

# Create build directory (clean first for idempotent re-runs)
rm -rf build-qpid
mkdir -p build-qpid
cd build-qpid

# Extract the included tarball
tar xf ../debian/qpid-proton/qpid-proton-0.40.0.tar.gz

# Build qpid-proton
mv qpid-proton-0.40.0 qpid-proton
cd qpid-proton
mkdir build
cd build

# Configure with cmake - ensure we build with pkg-config files
cmake .. -DCMAKE_INSTALL_PREFIX=/usr \
         -DSYSINSTALL_BINDINGS=ON \
         -DBUILD_STATIC_LIBS=ON \
         -DBUILD_PYTHON=OFF \
         -DBUILD_RUBY=OFF \
         -DBUILD_GO=OFF

# Honor DEB_BUILD_OPTIONS=parallel=N from package builder, else use nproc
MAKE_JOBS="-j$(echo "${DEB_BUILD_OPTIONS:-}" | sed -n 's/.*parallel=\([0-9]*\).*/\1/p')"
if [ -z "${MAKE_JOBS#-j}" ]; then
	MAKE_JOBS="-j$(nproc)"
fi
make $MAKE_JOBS

# Install to specified DESTDIR
make install DESTDIR="${DESTDIR}"

# Go back to original directory
cd ../../..
