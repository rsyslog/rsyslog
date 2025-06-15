$ModLoad
--------

**Type:** global configuration parameter

**Default:**

**Description:**

Dynamically loads a plug-in into rsyslog's address space and activates
it. The plug-in must obey the rsyslog module API. Currently, only MariaDB/
MySQL and Postgres output modules are available as a plugins, but users 
may create their own. A plug-in must be loaded BEFORE any configuration 
file lines that reference it.

Modules must be present in the system default destination for rsyslog
modules. You can also set the directory via the
`$ModDir <rsconf1_moddir.html>`_ parameter.

If a full path name is specified, the module is loaded from that path.
The default module directory is ignored in that case.

**Sample:**

.. code-block:: none

  $ModLoad ommysql # load MariaDB/MySQL functionality
  $ModLoad /rsyslog/modules/ompgsql.so # load the postgres module via absolute path


