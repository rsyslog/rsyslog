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
   * - :ref:`param-ommysql-mysqlconfig.file`
     - .. include:: ../../reference/parameters/ommysql-mysqlconfig.file.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-ommysql-mysqlconfig.section`
     - .. include:: ../../reference/parameters/ommysql-mysqlconfig.section.rst
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
   ../../reference/parameters/ommysql-mysqlconfig.file
   ../../reference/parameters/ommysql-mysqlconfig.section
   ../../reference/parameters/ommysql-template


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
