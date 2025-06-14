Installation
============

Installation is usually as simple as typing

|  ``$ sudo yum install rsyslog``,
|  ``$ sudo apt-get install rsyslog``, or
|  ``$ sudo apk add rsyslog``

Unfortunately distributions usually provide rather old versions of
rsyslog, and so there are chances you want to have something
newer. To do this easily, we provide :doc:`packages <packages>`
and :doc:`Docker containers <rsyslog_docker>`.

Alternatively you can build directly from source. That provides most
flexibility and control over the resulting binaries, but obviously also
requires most work. Some prior knowledge with building software on
your system is recommended.

.. toctree::
   :maxdepth: 2

   packages
   rsyslog_docker
   install_from_source
   build_from_repo
