`back <rsyslog_conf_global.html>`_

$GssMode
--------

**Type:** global configuration directive

**Default:** encryption

**Provided by:** *omgssapi*

**Description:**

Specifies GSS-API mode to use, which can be "**integrity**\ " - clients
are authenticated and messages are checked for integrity,
"**encryption**\ " - same as "integrity", but messages are also
encrypted if both sides support it.

**Sample:**

``$GssMode Encryption``

[`rsyslog.conf overview <rsyslog_conf.html>`_\ ] [`manual
index <manual.html>`_\ ] [`rsyslog site <http://www.rsyslog.com/>`_\ ]

This documentation is part of the `rsyslog <http://www.rsyslog.com/>`_
project.
 Copyright © 2007 by `Rainer Gerhards <http://www.gerhards.net/rainer>`_
and `Adiscon <http://www.adiscon.com/>`_. Released under the GNU GPL
version 2 or higher.
