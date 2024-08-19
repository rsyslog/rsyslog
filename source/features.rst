RSyslog - Features
==================

**This page lists both current features as well as those being
considered for future versions of rsyslog.** If you think a feature is
missing, drop `Rainer <mailto:rgerhards@adiscon.com>`_ a note. Rsyslog
is a vital project. Features are added each few days. If you would like
to keep up of what is going on, you can also subscribe to the `rsyslog
mailing list <http://lists.adiscon.net/mailman/listinfo/rsyslog>`_.

A better structured feature list is now contained in our `rsyslog vs.
syslog-ng comparison <rsyslog_ng_comparison.html>`_. Probably that page
will replace this one in the future.

Current Features
----------------

-  native support for `writing to MariaDB/MySQL databases <rsyslog_mysql.html>`_
-  native support for writing to Postgres databases
-  direct support for Firebird/Interbase, OpenTDS (MS SQL, Sybase),
   SQLite, Ingres, Oracle, and mSQL via libdbi, a database abstraction
   layer (almost as good as native)
-  native support for `sending mail messages <ommail.html>`_ (first seen
   in 3.17.0)
-  support for (plain) tcp based syslog - much better reliability
-  support for sending and receiving compressed syslog messages
-  support for on-demand on-disk spooling of messages that can not be
   processed fast enough (a great feature for `writing massive amounts
   of syslog messages to a database <rsyslog_high_database_rate.html>`_)
-  support for selectively `processing messages only during specific
   timeframes <http://wiki.rsyslog.com/index.php/OffPeakHours>`_ and
   spooling them to disk otherwise
-  ability to monitor text files and convert their contents into syslog
   messages (one per line)
-  ability to configure backup syslog/database servers - if the primary
   fails, control is switched to a prioritized list of backups
-  support for receiving messages via reliable `RFC
   3195 <http://www.monitorware.com/Common/en/glossary/rfc3195.php>`_
   delivery (a bit clumsy to build right now...)
-  ability to generate file names and directories (log targets)
   dynamically, based on many different properties
-  control of log output format, including ability to present channel
   and priority as visible log data
-  good timestamp format control; at a minimum, ISO 8601/RFC 3339
   second-resolution UTC zone
-  ability to reformat message contents and work with substrings
-  support for log files larger than 2gb
-  support for file size limitation and automatic rollover command
   execution
-  support for running multiple rsyslogd instances on a single machine
-  support for `TLS-protected syslog <rsyslog_tls.html>`_ (both
   `natively <rsyslog_tls.html>`_ and via
   `stunnel <rsyslog_stunnel.html>`_)
-  ability to filter on any part of the message, not just facility and
   severity
-  ability to use regular expressions in filters
-  support for discarding messages based on filters
-  ability to execute shell scripts on received messages
-  control of whether the local hostname or the hostname of the origin
   of the data is shown as the hostname in the output
-  ability to preserve the original hostname in NAT environments and
   relay chains
-  ability to limit the allowed network senders
-  powerful BSD-style hostname and program name blocks for easy
   multi-host support
-  massively multi-threaded with dynamic work thread pools that start up
   and shut themselves down on an as-needed basis (great for high log
   volume on multicore machines)
-  very experimental and volatile support for
   `syslog-protocol <syslog_protocol.html>`_ compliant messages (it is
   volatile because standardization is currently underway and this is a
   proof-of-concept implementation to aid this effort)
-  world's first implementation of syslog-transport-tls
-  the sysklogd's klogd functionality is implemented as the *imklog*
   input plug-in. So rsyslog is a full replacement for the sysklogd
   package
-  support for IPv6
-  ability to control repeated line reduction ("last message repeated n
   times") on a per selector-line basis
-  supports sub-configuration files, which can be automatically read
   from directories. Includes are specified in the main configuration
   file
-  supports multiple actions per selector/filter condition
-  MariaDB/MySQL and Postgres SQL functionality as a dynamically loadable
   plug-in
-  modular design for inputs and outputs - easily extensible via custom
   plugins
-  an easy-to-write to plugin interface
-  ability to send SNMP trap messages
-  ability to filter out messages based on sequence of arrival
-  support for comma-separated-values (CSV) output generation (via the
   "csv" property replace option). The CSV format supported is that from
   RFC 4180.
-  support for arbitrary complex boolean, string and arithmetic
   expressions in message filters

World's first
-------------

Rsyslog has an interesting number of "world's firsts" - things that were
implemented for the first time ever in rsyslog. Some of them are still
features not available elsewhere.

-  world's first implementation of IETF I-D syslog-protocol (February
   2006, version 1.12.2 and above), now RFC5424
-  world's first implementation of dynamic syslog on-the-wire
   compression (December 2006, version 1.13.0 and above)
-  world's first open-source implementation of a disk-queueing syslogd
   (January 2008, version 3.11.0 and above)
-  world's first implementation of IETF I-D syslog-transport-tls (May
   2008, version 3.19.0 and above)

Upcoming Features
-----------------

The list below is something like a repository of ideas we'd like to
implement. Features on this list are typically NOT scheduled for
immediate inclusion.

**Note that we also maintain a `list of features that are looking for
sponsors <http://www.rsyslog.com/sponsor_feature>`_. If you are
interested in any of these features, or any other feature, you may
consider sponsoring the implementation. This is also a great way to show
your commitment to the open source community. Plus, it can be
financially attractive: just think about how much less it may be to
sponsor a feature instead of purchasing a commercial implementation.
Also, the benefit of being recognized as a sponsor may even drive new
customers to your business!**

-  port it to more \*nix variants (eg AIX and HP UX) - this needs
   volunteers with access to those machines and knowledge
-  pcre filtering - maybe (depending on feedback)  - simple regex
   already partly added. So far, this seems sufficient so that there is
   no urgent need to do pcre. If done, it will be a loadable
   RainerScript function.
-  support for `RFC
   3195 <http://www.monitorware.com/Common/en/glossary/rfc3195.php>`_ as
   a sender - this is currently unlikely to happen, because there is no
   real demand for it. Any work on RFC 3195 has been suspend until we
   see some real interest in it.  It is probably much better to use
   TCP-based syslog, which is interoperable with a large number of
   applications. You may also read my blog post on the future of
   liblogging, which contains interesting information about the `future
   of RFC 3195 in
   rsyslog <http://rgerhards.blogspot.com/2007/09/where-is-liblogging-heading-to.html>`_.

To see when each feature was added, see the `rsyslog change
log <http://www.rsyslog.com/Topic4.phtml>`_ (online only).
