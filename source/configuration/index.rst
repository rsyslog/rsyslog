Configuration
=============

**Rsyslog Configuration Reference Manual Introduction**

This document serves as a detailed guide to rsyslog configuration, offering extensive information on the setup and management of system logging using
`rsyslog <https://www.rsyslog.com>`_
It covers various aspects of rsyslog configuration, including constructs, statements, and key concepts, designed to assist users in customizing their logging infrastructure according to specific needs.

The primary configuration file for rsyslog, located at `/etc/rsyslog.conf`, acts as the central point for establishing logging rules. This file is used to define input modules, filters, actions, and global directives, facilitating the processes of log collection, filtering, routing, and formatting.

Please note that this documentation is currently in the process of being refined to improve its clarity, structure, and accessibility. We value your patience and understanding during this phase and are committed to delivering a comprehensive and easy-to-navigate guide to rsyslog.

For further exploration of rsyslog's configuration intricacies, please refer to the links provided below. This manual is designed to be a valuable resource for both experienced system administrators and those new to the field, aiming to fully leverage the capabilities of rsyslog.

Note that **configurations can be built interactively** via the online
`rsyslog configuration builder <http://www.rsyslog.com/rsyslog-configuration-builder/>`_ tool.

.. toctree::
   :maxdepth: 2

   conf_formats
   converting_to_new_format
   sysklogd_format
   basic_structure
   templates
   properties
   property_replacer
   filters
   ../rainerscript/index
   actions
   input
   parser
   timezone
   examples
   index_directives
   rsyslog_statistic_counter
   modules/index
   output_channels
   droppriv
   ipv6
   cryprov_gcry
   dyn_stats
   lookup_tables
   percentile_stats

`Configuration file examples can be found in the rsyslog
wiki <http://wiki.rsyslog.com/index.php/Configuration_Samples>`_. Also
keep the `rsyslog config
snippets <http://www.rsyslog.com/config-snippets/>`_ on your mind. These
are ready-to-use real building blocks for rsyslog configuration.

There is also one sample file provided together with the documentation
set. If you do not like to read, be sure to have at least a quick look
at :download:`rsyslog-example.conf <rsyslog-example.conf>`.

While rsyslogd contains enhancements over standard syslogd, efforts have
been made to keep the configuration file as compatible as possible.
While, for obvious reasons, :doc:`enhanced features <../features>` require
a different config file syntax, rsyslogd should be able to work with a
standard syslog.conf file. This is especially useful while you are
migrating from syslogd to rsyslogd.
