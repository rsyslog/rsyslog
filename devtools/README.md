This directory contains tools for use by development and
CI. The goal is to have a consistent way to do things.

## Scripts

- run-static-analyzer.sh
  Run the currently approved rsyslog project clang static analyzer.

## Environment Variables

- RSYSLOG\_HOME
  root of rsyslog sources - e.g. unzipped tarball, git checkout

## Files

### default\_dev\_container

Contains the full name of the container to be used for development
task if no other is given.
