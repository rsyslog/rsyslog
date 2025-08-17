.. _install-rsyslog-docker:

Using Rsyslog Docker Containers
===============================

.. index::
   single: containers
   pair: installation; Docker

The rsyslog project publishes official Docker images on Docker Hub.
They are built from the definitions in ``packaging/docker`` and ship
current rsyslog releases.

Images follow the pattern ``rsyslog/rsyslog[-<function>]:<version>``
with variants such as ``rsyslog/rsyslog`` (standard) and
``rsyslog/rsyslog-minimal``. To build them locally run::

    make -C packaging/docker/rsyslog all

See :doc:`../containers/index` for an overview of available images and
for notes on development and historical container work.
