`rsyslog module reference <rsyslog_conf_modules.html>`_

stdout output module (stdout)
=============================

**Module Name:    omstdout**

**Author:**\ Rainer Gerhards <rgerhards@adiscon.com>

**Available Since**: 4.1.6

**Description**:

This module writes any messages that are passed to it to stdout. It was
developed for the rsyslog test suite. However, there may (limited) other
uses exists. Please not that we do not put too much effort into the
quality of this module as we do not expect it to be used in real
deployments. If you do, please drop us a note so that we can enhance its
priority!

**Configuration Directives**:

-  **$ActionOMStdoutArrayInterface** [on\|**off**
    This setting instructs omstdout to use the alternate array based
   method of parameter passing. If used, the values will be output with
   commas between the values but no other padding bytes. This is a test
   aid for the alternate calling interface.
-  **$ActionOMStdoutEnsureLFEnding** [**on**\ \|off
    Makes sure that each message is written with a terminating LF. This
   is needed for the automatted tests. If the message contains a
   trailing LF, none is added.

**Caveats/Known Bugs:**

Currently none known.

[`rsyslog.conf overview <rsyslog_conf.html>`_\ ] [`manual
index <manual.html>`_\ ] [`rsyslog site <http://www.rsyslog.com/>`_\ ]

This documentation is part of the `rsyslog <http://www.rsyslog.com/>`_
project.
 Copyright © 2009 by `Rainer Gerhards <http://www.gerhards.net/rainer>`_
and `Adiscon <http://www.adiscon.com/>`_. Released under the GNU GPL
version 3 or higher.
