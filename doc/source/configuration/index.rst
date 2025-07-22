Configuration
=============

This section is the **reference manual for configuring rsyslog**. It
covers all major configuration concepts, modules, and directives needed
to build robust logging infrastructures — from simple setups to complex
log processing pipelines.

rsyslog’s primary configuration file is located at:
``/etc/rsyslog.conf``

Additional configuration snippets are commonly placed in:
``/etc/rsyslog.d/*.conf``

Within these files, you define:
- **Input modules** (where logs come from)
- **Filters and parsers** (how logs are processed)
- **Actions** (where logs are sent)
- **Global directives** (overall behavior and performance tuning)

The topics listed below provide a complete guide to rsyslog
configuration.

.. toctree::
   :maxdepth: 2

   basic_structure
   modules/idx_output
   modules/idx_input
   modules/idx_parser
   modules/idx_messagemod
   modules/idx_stringgen
   modules/idx_library
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
   cryprov_ossl
   dyn_stats
   lookup_tables
   percentile_stats
   converting_to_new_format
   conf_formats
   sysklogd_format

Additional Resources
--------------------

- **Config snippets:** See `rsyslog config snippets
  <http://www.rsyslog.com/config-snippets/>`_ for ready-to-use building
  blocks.

- **Example configuration:** Download a sample configuration file:
  :download:`rsyslog-example.conf <rsyslog-example.conf>`.

Compatibility Note
------------------

rsyslog retains partial configuration compatibility with traditional
BSD-style syslogd, which can be helpful when migrating from older
implementations (e.g., on Solaris or AIX). On modern Linux systems,
native rsyslog configuration formats (especially RainerScript) are
recommended and provide access to all advanced features.
