Rsyslog Configuration
=====================

**Rsyslogd is configured via the rsyslog.conf file**, typically found in
/etc. By default, rsyslogd reads the file /etc/rsyslog.conf. This may be
changed by a command line option.

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
While, for obvious reasons, :doc:`enhanced features <features>` require
a different config file syntax, rsyslogd should be able to work with a
standard syslog.conf file. This is especially useful while you are
migrating from syslogd to rsyslogd.


.. toctree::
   :maxdepth: 2

   global_index
   nomatch
   filters
   actions
   templates
   output_channels
   examples
   modules/index
   global/index
