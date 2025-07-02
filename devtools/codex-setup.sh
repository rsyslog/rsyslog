#!/usr/bin/env bash
##
## rsyslog base environment setup script
##
## Installs required build tools and libraries once,
## then records completion in /tmp/rsyslog_base_env.flag
##
## Usage:
##   ./setup-env.sh
##

FLAG="/tmp/rsyslog_base_env.flag"

## If the flag exists, skip setup
if [ -f "$FLAG" ]; then
  echo "Base environment already set up (flag at $FLAG), skipping."
  exit 0
fi

## Perform package update and installation
echo "Setting up rsyslog build environment..."
apt-get update
apt-get install -y \
  autoconf \
  autoconf-archive \
  automake \
  autotools-dev \
  bison \
  flex \
  gcc \
  libtool \
  libtool-bin \
  make \
  libcurl4-gnutls-dev \
  libgcrypt20-dev \
  libgnutls28-dev \
  libestr-dev \
  libfastjson-dev \
  liblognorm-dev \
  libaprutil1-dev \
  libcivetweb-dev \
  python3-docutils \
  valgrind

## Record that setup has completed
touch "$FLAG"
echo "Base environment setup complete. Flag created at $FLAG."
