
FAQ: Encrypting MySQL Traffic with ommysql Plugin
==================================================

Question
--------
I'm using the ommysql plugin to write log entries to a MariaDB. Is it possible to encrypt the MySQL traffic on port 3306?

Answer
------
Yes, it is possible to encrypt the MySQL traffic on port 3306 when using the `ommysql` plugin with rsyslog. This is configurable through the MySQL or MariaDB configuration file specified by the `MySQLConfig.File` parameter.

Steps to Enable TLS
-------------------

1. **Configure MySQL/MariaDB Server**
   - Update your MySQL or MariaDB configuration file (usually `my.cnf` or `my.ini`) to enable SSL/TLS.
   - Add the following parameters to specify the CA certificate, server certificate, and server key::

     [mysqld]
     ssl-ca=/path/to/ca-cert.pem
     ssl-cert=/path/to/server-cert.pem
     ssl-key=/path/to/server-key.pem

   - Ensure these paths point to the correct certificate files on your server.

2. **Update rsyslog Configuration**
   - Configure rsyslog to use the `ommysql` plugin and specify the MySQL configuration file that includes the TLS settings::

     module(load="ommysql")

     action(
         type="ommysql"
         server="your-mariadb-server"
         serverport="3306"
         db="your-database"
         uid="your-username"
         pwd="your-password"
         mysqlconfig.file="/path/to/my.cnf"
     )

Additional Resources
--------------------
- `MariaDB SSL/TLS Documentation <https://mariadb.com/kb/en/securing-connections-for-client-and-server/>`_
- `rsyslog ommysql Documentation <https://www.rsyslog.com/doc/master/configuration/modules/ommysql.html>`_
