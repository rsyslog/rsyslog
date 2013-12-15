`back <rsyslog_conf_modules.html>`_

/dev/kmsg Log Input Module
==========================

**Module Name:    imkmsg**

**Authors:**\ Rainer Gerhards <rgerhards@adiscon.com>
 Milan Bartos <mbartos@redhat.com>

**Description**:

Reads messages from the /dev/kmsg structured kernel log and submits them
to the syslog engine.

The printk log buffer constains log records. These records are exported
by /dev/kmsg device as structured data in the following format:
 "level,sequnum,timestamp;<message text>\\n"
 There could be continuation lines starting with space that contains
key/value pairs.
 Log messages are parsed as necessary into rsyslog msg\_t structure.
Continuation lines are parsed as json key/value pairs and added into
rsyslog's message json representation.

**Configuration Directives**:

This module has no configuration directives.

**Caveats/Known Bugs:**

This module can't be used together with imklog module. When using one of
them, make sure the other one is not enabled.

This is Linux specific module and requires /dev/kmsg device with
structured kernel logs.

**Sample:**

The following sample pulls messages from the /dev/kmsg log device. All
parameters are left by default, which is usually a good idea. Please
note that loading the plugin is sufficient to activate it. No directive
is needed to start pulling messages.

$ModLoad imkmsg

[`rsyslog.conf overview <rsyslog_conf.html>`_\ ] [`manual
index <manual.html>`_\ ] [`rsyslog site <http://www.rsyslog.com/>`_\ ]

This documentation is part of the `rsyslog <http://www.rsyslog.com/>`_
project.
 Copyright © 2008-2009 by `Rainer
Gerhards <http://www.gerhards.net/rainer>`_ and
`Adiscon <http://www.adiscon.com/>`_. Released under the GNU GPL version
3 or higher.
