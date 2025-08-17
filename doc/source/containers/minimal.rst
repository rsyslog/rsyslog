.. _containers-user-minimal:

rsyslog/rsyslog-minimal
=======================

A lean Ubuntu-based image containing the rsyslog core and a tiny
configuration that writes logs to standard output. It serves as the
foundation for the other rsyslog images and is suitable when you want to
add your own modules or configuration.

Environment Variables
---------------------

The entrypoint script recognises the following variables:

- ``RSYSLOG_HOSTNAME`` – hostname used inside rsyslog. If unset the
  script reads ``/etc/hostname``.
- ``PERMIT_UNCLEAN_START`` – skip ``rsyslogd -N1`` configuration check
  when set. By default the configuration is validated.
- ``RSYSLOG_ROLE`` – role name consumed by the entrypoint. Defaults to
  ``minimal`` and normally does not need to be changed.
