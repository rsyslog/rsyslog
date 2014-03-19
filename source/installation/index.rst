Installation
============

Installation is usually as simple as saying

``$ sudo yum install rsyslog``

or

``$ sudo apt-get install rsyslog``

However, distributions usually provide rather old versions of
rsyslog, and so there are chances you want to have something
newer. In that case, you need to either use the rsyslog project's
own packages, a distribution tarball or build directly from repo.
All of this is detailled in this chapter.

.. toctree::
   :maxdepth: 2

   packages
   install_from_source
   build_from_repo
