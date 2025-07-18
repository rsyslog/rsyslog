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

   Parameter names are case-insensitive.


Action Parameters
-----------------

Conninfo
^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "none", "no", "none"

The URI or set of key-value pairs that describe how to connect to the PostgreSQL
server. This takes precedence over ``server``, ``port``, ``db``, and ``pass``
parameters. Required if ``server`` and ``db`` are not specified.

The format corresponds to `standard PostgreSQL connection string format
<https://www.postgresql.org/docs/current/libpq-connect.html#LIBPQ-CONNSTRING>`_.

Server
^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "none", "no", "none"

The hostname or address of the PostgreSQL server. Required if ``conninfo`` is
not specified.


Port/Serverport
^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "5432", "no", "none"

The IP port of the PostgreSQL server.


db
^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "none", "no", "none"

The multi-tenant database name to ``INSERT`` rows into. Required if ``conninfo``
is not specified.


User/UID
^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "postgres", "no", "none"

The username to connect to the PostgreSQL server with.


Pass/PWD
^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "postgres", "no", "none"

The password to connect to the PostgreSQL server with.


Template
^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "none", "no", "none"

The template name to use to ``INSERT`` rows into the database with. Valid SQL
syntax is required, as the module does not perform any insertion statement
checking.


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


