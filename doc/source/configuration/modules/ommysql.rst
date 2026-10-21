.. SPDX-License-Identifier: Apache-2.0

*********************************************
ommysql: MariaDB/MySQL Database Output Module
*********************************************

===========================  ===========================================================================
**Module Name:**             **ommysql**
**Author:**                  Michael Meckelein (Initial Author) / `Rainer Gerhards <https://rainer.gerhards.net/>`_ <rgerhards@adiscon.com>
===========================  ===========================================================================


Purpose
=======

This module provides native support for logging to MariaDB/MySQL 
databases. It offers superior performance over the more generic
`omlibdbi <omlibdbi.html>`_ module.


Configuration Parameters
========================

.. note::

   Parameter names are case-insensitive; camelCase is recommended for readability.


Action Parameters
-----------------

.. list-table::
   :widths: 30 70
   :header-rows: 1

   * - Parameter
     - Summary
   * - :ref:`param-ommysql-server`
     - .. include:: ../../reference/parameters/ommysql-server.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-ommysql-socket`
     - .. include:: ../../reference/parameters/ommysql-socket.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-ommysql-db`
     - .. include:: ../../reference/parameters/ommysql-db.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-ommysql-uid`
     - .. include:: ../../reference/parameters/ommysql-uid.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-ommysql-pwd`
     - .. include:: ../../reference/parameters/ommysql-pwd.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-ommysql-serverport`
     - .. include:: ../../reference/parameters/ommysql-serverport.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-ommysql-mysqlconfig-file`
     - .. include:: ../../reference/parameters/ommysql-mysqlconfig-file.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-ommysql-mysqlconfig-section`
     - .. include:: ../../reference/parameters/ommysql-mysqlconfig-section.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-ommysql-template`
     - .. include:: ../../reference/parameters/ommysql-template.rst
        :start-after: .. summary-start
        :end-before: .. summary-end

.. toctree::
   :hidden:

   ../../reference/parameters/ommysql-server
   ../../reference/parameters/ommysql-socket
   ../../reference/parameters/ommysql-db
   ../../reference/parameters/ommysql-uid
   ../../reference/parameters/ommysql-pwd
   ../../reference/parameters/ommysql-serverport
   ../../reference/parameters/ommysql-mysqlconfig-file
   ../../reference/parameters/ommysql-mysqlconfig-section
   ../../reference/parameters/ommysql-template


Failure and retry behavior
==========================

``ommysql`` treats MariaDB/MySQL client and connection failures differently
from SQL data errors returned by the server.

Client-side connection failures, such as an unavailable server or a broken
connection, cause ``ommysql`` to reconnect and retry the failed statement once.
If reconnecting or retrying fails, the action is suspended. In that state, the
usual action retry settings, including ``action.resumeInterval`` and
``action.resumeRetryCount``, control when rsyslog tries to resume delivery.

Server-side SQL errors, such as a rejected ``INSERT`` or stored procedure call,
are treated as permanent data failures for the affected messages. rsyslog logs
the MySQL error and the failed SQL statement, then handles the failed message
through the action error-file path. These failures are not retried (and thus not
throttled by ``action.resumeInterval``) because retrying the same malformed
statement later would normally fail in the same way. MySQL 8.0.24 and later has
one connection-related exception at the start of a transaction: error 4031
(``ER_CLIENT_INTERACTION_TIMEOUT``) reports that the server disconnected an
idle client due to ``wait_timeout`` or ``interactive_timeout``. When this error
is returned by ``START TRANSACTION``, ``ommysql`` reconnects and retries that
operation once, before any message SQL in that transaction has run. It does not
retry error 4031 for later SQL statements: reconnecting then could discard
earlier statements from the same transaction. MariaDB uses error 4031 for a
different SQL error, which remains a permanent data failure.

For production setups, configure ``action.errorfile`` on the ``ommysql`` action
when failed messages must be retained for inspection or replay. Without an error
file, permanently failed messages are discarded after rsyslog reports the action
failure.


Examples
========

Example 1
---------

The following sample writes all syslog messages to the database
"syslog_db" on mysqlserver.example.com. The server is being accessed
under the account of "user" with password "pwd".

.. code-block:: none

   module(load="ommysql")
   action(type="ommysql" server="mysqlserver.example.com" serverPort="1234"
          db="syslog_db" uid="user" pwd="pwd")



FAQ
===

* :doc:`How can I encrypt the rsyslog connection to MariaDB/MySQL? <../../faq/encrypt_mysql_traffic_ommysql>`
