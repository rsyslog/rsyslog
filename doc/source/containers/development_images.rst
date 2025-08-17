.. _containers-development-images:

Development and Historical Images
---------------------------------

.. index::
   pair: containers; development images
   pair: containers; historical images

Large images used by rsyslog's Continuous Integration live under
``packaging/docker/dev_env``. They provide all build dependencies for
multiple distributions (Alpine, CentOS, Debian, Fedora, SUSE and
Ubuntu) and are intended for contributors who need to reproduce the CI
environment. Due to the many tools they include, these images are
several gigabytes in size and are not meant for production use.

The repository also retains some historical material for reference:

* ``packaging/docker/appliance`` – deprecated attempt at an all-in-one
  logging appliance.
* ``packaging/docker/base`` – obsolete base images (Alpine and CentOS 7).

These directories are kept to document past work but are no longer
maintained.
