.. _containers-user-images:

User-Focused Images
-------------------

.. index::
   pair: containers; user images
   pair: Docker; rsyslog images

Official images are built on Ubuntu LTS releases and install the latest
rsyslog packages from the Adiscon daily-stable PPA. Images are tagged
``rsyslog/rsyslog[-<function>]:<version>`` where ``<version>`` follows
the ``YYYY-MM`` rsyslog release.

Available variants include:

* :doc:`rsyslog/rsyslog-minimal <minimal>` – rsyslog core only.
* :doc:`rsyslog/rsyslog <standard>` – standard image with common modules
  such as ``imhttp`` and ``omhttp``.
* :doc:`rsyslog/rsyslog-collector <collector>` – adds modules for
  centralized log collection (``elasticsearch``, ...).
* :doc:`rsyslog/rsyslog-dockerlogs <dockerlogs>` – includes ``imdocker``
  to process logs from the Docker daemon.
* ``rsyslog/rsyslog-debug`` – planned variant with troubleshooting tools.

.. toctree::
   :maxdepth: 1

   minimal
   standard
   collector
   dockerlogs

Images are built using the layered ``Makefile`` in
``packaging/docker/rsyslog``::

    make all
    make standard
    make VERSION=2025-06 minimal

Configuration snippets in each directory and environment variables
allow enabling or disabling functionality at runtime. Consult the
`packaging/docker README <https://github.com/rsyslog/rsyslog/tree/main/packaging/docker>`_
for the latest details and feedback instructions.
