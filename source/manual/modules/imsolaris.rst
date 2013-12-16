`back <rsyslog_conf_modules.html>`_

Solaris Input Module
====================

**Module Name:    imsolaris**

**Author:**\ Rainer Gerhards <rgerhards@adiscon.com>

**Description**:

Reads local Solaris log messages including the kernel log.

This module is specifically tailored for Solaris. Under Solaris, there
is no special kernel input device. Instead, both kernel messages as well
as messages emitted via syslog() are received from a single source.

This module obeys the Solaris door() mechanism to detect a running
syslogd instance. As such, only one can be active at one time. If it
detects another active intance at startup, the module disables itself,
but rsyslog will continue to run.

**Configuration Directives**:

-  **$IMSolarisLogSocketName <name>**
    This is the name of the log socket (stream) to read. If not given,
   /dev/log is read.

**Caveats/Known Bugs:**

None currently known. For obvious reasons, works on Solaris, only (and
compilation will most probably fail on any other platform).

**Sample:**

The following sample pulls messages from the default log source

$ModLoad imsolaris

[`rsyslog.conf overview <rsyslog_conf.html>`_\ ] [`manual
index <manual.html>`_\ ] [`rsyslog site <http://www.rsyslog.com/>`_\ ]

This documentation is part of the `rsyslog <http://www.rsyslog.com/>`_
project.
 Copyright © 2010 by `Rainer Gerhards <http://www.gerhards.net/rainer>`_
and `Adiscon <http://www.adiscon.com/>`_. Released under the GNU GPL
version 3 or higher.
