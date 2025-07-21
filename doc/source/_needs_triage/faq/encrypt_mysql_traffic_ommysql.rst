.. index:: ! encrypt mysql traffic

Encrypting MySQL Traffic with ommysql
=====================================

This document describes how to encrypt traffic between rsyslog and MySQL/MariaDB.

Overview
--------

When using ommysql to write to a MySQL or MariaDB database, you may want to
encrypt the connection for security reasons.

Configuration
-------------

To enable encryption, you need to configure both the MySQL server and rsyslog.

MySQL Server Configuration
^^^^^^^^^^^^^^^^^^^^^^^^^^

Ensure your MySQL server is configured to require SSL connections.

Rsyslog Configuration
^^^^^^^^^^^^^^^^^^^^^

Use the following configuration to enable SSL:

.. code-block:: rsyslog

   module(load="ommysql")
   
   action(type="ommysql"
          server="localhost"
          db="Syslog"
          uid="rsyslog"
          pwd="password"
          ssl="on")

Security Notes
--------------

- Always use strong passwords
- Consider using certificate-based authentication
- Regularly update your MySQL server and rsyslog 