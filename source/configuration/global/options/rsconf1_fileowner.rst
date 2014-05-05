`back <rsyslog_conf_global.html>`_

$FileOwner
----------

**Type:** global configuration directive

**Default:**

**Description:**

Set the file owner for dynaFiles newly created. Please note that this
setting does not affect the owner of files already existing. The
parameter is a user name, for which the userid is obtained by rsyslogd
during startup processing. Interim changes to the user mapping are not
detected.

**Sample:**

``$FileOwner loguser``

[`rsyslog.conf overview <rsyslog_conf.html>`_\ ] [`manual
index <manual.html>`_\ ] [`rsyslog site <http://www.rsyslog.com/>`_\ ]

This documentation is part of the `rsyslog <http://www.rsyslog.com/>`_
project.
Copyright © 2007 by `Rainer Gerhards <http://www.gerhards.net/rainer>`_
and `Adiscon <http://www.adiscon.com/>`_. Released under the GNU GPL
version 2 or higher.
