# rsyslog-docker
a playground for rsyslog docker tasks - nothing production yet

see also https://github.com/rsyslog/rsyslog/projects/5

The docker effort currently uses multiple containers.

# Alpine Linux
We intend to use alpine linux for the logging appliance container, because
it is small, secure and relatively recently.

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
