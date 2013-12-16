rsyslog.conf configuration file
===============================

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
at `rsyslog-example.conf <rsyslog-example.conf>`_.

While rsyslogd contains enhancements over standard syslogd, efforts have
been made to keep the configuration file as compatible as possible.
While, for obvious reasons, `enhanced features <features.html>`_ require
a different config file syntax, rsyslogd should be able to work with a
standard syslog.conf file. This is especially useful while you are
migrating from syslogd to rsyslogd.

`Modules <rsyslog_conf_modules.html>`_
--------------------------------------

Lines
-----

Lines can be continued by specifying a backslash ("\\") as the last
character of the line. There is a hard-coded maximum line length of 4K.
If you need lines larger than that, you need to change compile-time
settings inside rsyslog and recompile.

`Configuration Directives <rsyslog_conf_global.html>`_
------------------------------------------------------

Basic Structure
---------------

Rsyslog supports standard sysklogd's configuration file format and
extends it. So in general, you can take a "normal" syslog.conf and use
it together with rsyslogd. It will understand everything. However, to
use most of rsyslogd's unique features, you need to add extended
configuration directives.

Rsyslogd supports the classical, selector-based rule lines. They are
still at the heart of it and all actions are initiated via rule lines. A
rule lines is any line not starting with a $ or the comment sign (#).
Lines starting with $ carry rsyslog-specific directives.

Every rule line consists of two fields, a selector field and an action
field. These two fields are separated by one or more spaces or tabs. The
selector field specifies a pattern of facilities and priorities
belonging to the specified action.
 Lines starting with a hash mark ("#'') and empty lines are ignored.

`Templates <rsyslog_conf_templates.html>`_
------------------------------------------

`Output Channels <rsyslog_conf_output.html>`_
---------------------------------------------

`Filter Conditions <rsyslog_conf_filter.html>`_
-----------------------------------------------

`Actions <rsyslog_conf_actions.html>`_
--------------------------------------

`Examples <rsyslog_conf_examples.html>`_
----------------------------------------

Here you will find examples for templates and selector lines. I hope
they are self-explanatory. If not, please see
www.monitorware.com/rsyslog/ for advise.

Configuration File Syntax Differences
-------------------------------------

Rsyslogd uses a slightly different syntax for its configuration file
than the original BSD sources. Originally all messages of a specific
priority and above were forwarded to the log file. The modifiers "='',
"!'' and "!-'' were added to make rsyslogd more flexible and to use it
in a more intuitive manner.
 The original BSD syslogd doesn't understand spaces as separators
between the selector and the action field.
 When compared to syslogd from sysklogd package, rsyslogd offers
additional `features <features.html>`_ (like template and database
support). For obvious reasons, the syntax for defining such features is
available in rsyslogd, only.

[`back to top <rsyslog_conf.html>`_\ ] [`manual index <manual.html>`_\ ]
[`rsyslog site <http://www.rsyslog.com/>`_\ ]

This documentation is part of the `rsyslog <http://www.rsyslog.com/>`_
project.
 Copyright © 2008-2011 by `Rainer
Gerhards <http://www.gerhards.net/rainer>`_ and
`Adiscon <http://www.adiscon.com/>`_. Released under the GNU GPL version
3 or higher.

>
