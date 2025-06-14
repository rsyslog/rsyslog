.. note::

   The rsyslog project maintains multiple `rsyslog docker development
   environment images <https://hub.docker.com/u/rsyslog/>`_. These
   images have been configured specifically for use with rsyslog and are
   recommended over your own build environment. Rsyslog docker development
   images are named with the ``rsyslog_dev_`` prefix, followed by the
   distro name.

.. warning::

   If you plan to copy the binary for use outside of the container you need
   to make sure to use an image of the same distro/version when building
   rsyslog.
