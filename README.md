# rsyslog-docker
a playground for rsyslog docker tasks - nothing production yet

see also https://github.com/rsyslog/rsyslog/projects/5

The docker effort currently uses multiple containers.

## Repository Overview
This repository contains various experimental Dockerfiles for building rsyslog containers.

* `/appliance` - an early experiment at a logging appliance. This approach did not work out and the images are outdated. The files are kept for reference only and should not be used.
* `/dev_env` - build environments used for rsyslog CI testing. They contain every dependency and are several gigabytes in size. These images are meant for contributors, not general public use.
* We are working on new images intended for general users. They will include minimal, standard and role-based variants (e.g. collector or docker). It is undecided whether these will replace `/base` or live under a new `/user` directory.
* Legacy Alpine and CentOS base images remain in `/base` for reference but are no longer maintained.

## Ubuntu Base
Development now focuses on Ubuntu images which integrate best with our daily stable package builds. Alpine and CentOS containers are no longer produced.

## History
### CentOS Notes
Older CentOS 7 definitions can be found under `/base/centos7`. Like the Alpine files, these are no longer maintained but remain for reference.
### Historical: Alpine Linux
The following notes describe the original Alpine based build process. These images are no longer built.
For those interested, the old packaging repository is still available at
https://github.com/rgerhards/alpine-rsyslog-extras.
We intended to use Alpine Linux for the logging appliance container because it is small and secure.

Right now, alpine misses some components that we need. So we build some
packages ourself. This will most probably be an ongoing activity as
we intend to always provide current versions of rsyslog inside the logging
application and it looks unlikely alpine will always follow exactly.

## Package Build Environment
We use

https://github.com/rgerhards/docker-alpine-abuild/tree/master-rger

to build alpine packages.

This is based on https://github.com/andyshinn/docker-alpine-abuild and
only adds a few rsyslog-specific tweaks. Most importantly, it has our
own unofficial APK repository enabled (we need this if we need to build
based on dependencies newer than there are in the official alpine
repositories).

Our **custom packages** are contained inside the

https://github.com/rgerhards/alpine-rsyslog-extras

repository.

### Bootstrap
Note: *usr* below stands for your user prefix.

To start from scratch, do

* create usr/docker-alpine-abuild image
  You need to hand-edit it so that the initial build does **not** use
  your custom repository.

* create autotools-archive package via usr/alpine-linux-extras
  - cd autotools-archive
  - source ../run
  Note: there asre some errors in regard to git repository mount point.
  So far, I do not know where the stem from and how to get rid of them.
  Fortunately, they do not harm the build process. Ignore them (and send
  us a PR if you know how to solve this cleanly).

* copy package to your intended destination http server

* rebuild usr/docker-alpine-abuild image
  reset your hand-edited change, make it use the custom repository again
  This is important as we need to have the dependencies for future builds.

* rebuild the rest of the packages in usr/alpine-linux-extras
  We don't want to give the exact sequence here as it might change.
  In general, rsyslog should be built last. You may need to do multiple
  uploads to your repo when these dependencies are needed by packages.

* **Remember to periodically apply (security) updates to the docker
  images!**
