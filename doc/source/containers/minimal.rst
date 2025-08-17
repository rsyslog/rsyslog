.. _containers-user-minimal:
.. _container.image.rsyslog-minimal:

rsyslog/rsyslog-minimal
=======================

.. index::
   pair: containers; rsyslog/rsyslog-minimal
   pair: Docker images; rsyslog/rsyslog-minimal

A lean Ubuntu-based image containing the rsyslog core and a tiny
configuration that writes logs to standard output. It serves as the
foundation for the other rsyslog images and is suitable when you want to
add your own modules or configuration.

Environment Variables
---------------------

The entrypoint script recognizes the following variables:

.. _containers-user-minimal-rsyslog_hostname:
.. envvar:: RSYSLOG_HOSTNAME

   Hostname used inside rsyslog. Defaults to the value of ``/etc/hostname`` when unset.

.. _containers-user-minimal-permit_unclean_start:
.. envvar:: PERMIT_UNCLEAN_START

   Skip configuration validation when set. By default ``rsyslogd -N1`` validates the configuration.

.. _containers-user-minimal-rsyslog_role:
.. envvar:: RSYSLOG_ROLE

   Role name consumed by the entrypoint. Defaults to ``minimal`` and normally does not need to be changed.
