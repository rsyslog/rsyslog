.. index:: ! ompgsql

*******************************************
PostgreSQL Database Output Module (ompgsql)
*******************************************

================  ==========================================================================
**Module Name:**  ompgsql
**Author:**       `Rainer Gerhards <rgerhards@adiscon.com>`__ and `Dan Molik <dan@danmolik.com>`__
**Available:**    8.32+
================  ==========================================================================


Purpose
=======

This module provides native support for logging to PostgreSQL databases.
It's an alternative (with potentially superior performance) to the more
generic :doc:`omlibdbi <omlibdbi>` module.


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
   * - :ref:`param-ompgsql-conninfo`
     - .. include:: ../../reference/parameters/ompgsql-conninfo.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-ompgsql-server`
     - .. include:: ../../reference/parameters/ompgsql-server.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-ompgsql-port`
     - .. include:: ../../reference/parameters/ompgsql-port.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-ompgsql-db`
     - .. include:: ../../reference/parameters/ompgsql-db.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-ompgsql-user`
     - .. include:: ../../reference/parameters/ompgsql-user.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-ompgsql-pass`
     - .. include:: ../../reference/parameters/ompgsql-pass.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-ompgsql-template`
     - .. include:: ../../reference/parameters/ompgsql-template.rst
        :start-after: .. summary-start
        :end-before: .. summary-end

.. toctree::
   :hidden:

   ../../reference/parameters/ompgsql-conninfo
   ../../reference/parameters/ompgsql-server
   ../../reference/parameters/ompgsql-port
   ../../reference/parameters/ompgsql-db
   ../../reference/parameters/ompgsql-user
   ../../reference/parameters/ompgsql-pass
   ../../reference/parameters/ompgsql-template


Examples
========

Example 1
---------

A Basic Example using the internal PostgreSQL template.

.. code-block:: none

   # load module
   module(load="ompgsql")

   action(type="ompgsql" server="localhost"
          user="rsyslog" pass="test1234"
          db="syslog")


Example 2
---------

A Basic Example using the internal PostgreSQL template and connection using URI.

.. code-block:: none

   # load module
   module(load="ompgsql")

   action(type="ompgsql"
          conninfo="postgresql://rsyslog:test1234@localhost/syslog")


Example 3
---------

A Basic Example using the internal PostgreSQL template and connection with TLS using URI.

.. code-block:: none

   # load module
   module(load="ompgsql")

   action(type="ompgsql"
          conninfo="postgresql://rsyslog:test1234@postgres.example.com/syslog?sslmode=verify-full&sslrootcert=/path/to/cert")


Example 4
---------

A Templated example.

.. code-block:: none

   template(name="sql-syslog" type="list" option.stdsql="on") {
     constant(value="INSERT INTO SystemEvents (message, timereported) values ('")
     property(name="msg")
     constant(value="','")
     property(name="timereported" dateformat="pgsql" date.inUTC="on")
     constant(value="')")
   }

   # load module
   module(load="ompgsql")

   action(type="ompgsql" server="localhost"
          user="rsyslog" pass="test1234"
          db="syslog"
          template="sql-syslog" )


Example 5
---------

An action queue and templated example.

.. code-block:: none

   template(name="sql-syslog" type="list" option.stdsql="on") {
     constant(value="INSERT INTO SystemEvents (message, timereported) values ('")
     property(name="msg")
     constant(value="','")
     property(name="timereported" dateformat="pgsql" date.inUTC="on")
     constant(value="')")
   }

   # load module
   module(load="ompgsql")

   action(type="ompgsql" server="localhost"
          user="rsyslog" pass="test1234"
          db="syslog"
          template="sql-syslog" 
          queue.size="10000" queue.type="linkedList"
          queue.workerthreads="5"
          queue.workerthreadMinimumMessages="500"
          queue.timeoutWorkerthreadShutdown="1000"
          queue.timeoutEnqueue="10000")


Building
========

To compile Rsyslog with PostgreSQL support you will need to:

* install *libpq* and *libpq-dev* packages, check your package manager for the correct name.
* set *--enable-pgsql* switch on configure.


