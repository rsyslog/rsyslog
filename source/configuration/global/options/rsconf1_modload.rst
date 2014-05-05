`back <rsyslog_conf_global.html>`_

$ModLoad
--------

**Type:** global configuration directive

**Default:**

**Description:**

Dynamically loads a plug-in into rsyslog's address space and activates
it. The plug-in must obey the rsyslog module API. Currently, only MySQL
and Postgres output modules are available as a plugins, but users may
create their own. A plug-in must be loaded BEFORE any configuration file
lines that reference it.

Modules must be present in the system default destination for rsyslog
modules. You can also set the directory via the
`$ModDir <rsconf1_moddir.html>`_ directive.

If a full path name is specified, the module is loaded from that path.
The default module directory is ignored in that case.

**Sample:**

``$ModLoad ommysql # load MySQL functionality $ModLoad /rsyslog/modules/ompgsql.so # load the postgres module via absolute path``

[`rsyslog.conf overview <rsyslog_conf.html>`_\ ] [`manual
index <manual.html>`_\ ] [`rsyslog site <http://www.rsyslog.com/>`_\ ]

This documentation is part of the `rsyslog <http://www.rsyslog.com/>`_
project.

Copyright © 2007 by `Rainer Gerhards <http://www.gerhards.net/rainer>`_
and `Adiscon <http://www.adiscon.com/>`_. Released under the GNU GPL
version 2 or higher.
