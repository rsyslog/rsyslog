rsyslog.conf configuration file
===============================

**Rsyslog is configured via the rsyslog.conf file**, typically found in
/etc. By default, rsyslogd reads the file /etc/rsyslog.conf. This may be
changed by command line option "-f".

`Configuration file examples can be found in the rsyslog
wiki <http://wiki.rsyslog.com/index.php/Configuration_Samples>`_. Also
keep the `rsyslog config
snippets <http://www.rsyslog.com/config-snippets/>`_ on your mind. These
are ready-to-use real building blocks for rsyslog configuration.

While rsyslogd contains enhancements over standard syslogd, efforts have
been made to keep the configuration file as compatible as possible.
While, for obvious reasons, `enhanced features <features.html>`_ require
a different config file syntax, rsyslogd should be able to work with a
standard syslog.conf file. This is especially useful while you are
migrating from syslogd to rsyslogd.

**Follow the links below to learn more about specific topics:**

-  `Basic Structure <rsyslog_conf_basic_structure.html>`_
-  `Modules <rsyslog_conf_modules.html>`_
-  `Templates <rsyslog_conf_templates.html>`_
-  `Filter Conditions <rsyslog_conf_filter.html>`_
-  `Actions (legacy format) <rsyslog_conf_actions.html>`_
-  `Output Channels <rsyslog_conf_output.html>`_
-  `Legacy Configuration Directives <rsyslog_conf_global.html>`_
-  `sysklogd compatibility <rsyslog_conf_sysklogd_compatibility.html>`_

[`back to top <rsyslog_conf.html>`_\ ] [`manual index <manual.html>`_\ ]
[`rsyslog site <http://www.rsyslog.com/>`_\ ]

This documentation is part of the `rsyslog <http://www.rsyslog.com/>`_
project.
 Copyright © 2008-2013 by `Rainer
Gerhards <http://www.gerhards.net/rainer>`_ and
`Adiscon <http://www.adiscon.com/>`_. Released under the GNU GPL version
3 or higher.
