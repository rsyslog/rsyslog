..
  :manpage_url: https://www.rsyslog.com/doc/man/rsyslogd.8.html

rsyslogd
########

:program:`rsyslogd` - reliable and extended syslogd

..
  This is the rst source for the rsyslogd man page.
  It is generated as part of the rsyslog build process.

.. program:: rsyslogd

.. sectionauthor:: Rainer Gerhards <rgerhards@adiscon.com>

Synopsis
--------

::

   rsyslogd [OPTION]...

Description
-----------

Rsyslogd is a system utility providing support for message logging.
Support of both internet and unix domain sockets enables this
utility to support both local and remote logging.

**Note:** This man page documentation is being migrated to the new
documentation structure. Please refer to the main documentation for
complete information.

Options
-------

.. option:: -D

   Run in debug mode. This prevents forking and provides additional
   debug output.

.. option:: -f config

   Specify an alternative configuration file instead of
   /etc/rsyslog.conf, which is the default.

.. option:: -N level

   Do a configuration check. Do NOT run in regular mode, just check
   configuration file correctness.

See Also
--------

:manpage:`rsyslog.conf(5)`

The complete documentation can be found in the rsyslog documentation.