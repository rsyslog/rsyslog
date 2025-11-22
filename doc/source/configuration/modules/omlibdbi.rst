****************************************
omlibdbi: Generic Database Output Module
****************************************

===========================  ===========================================================================
**Module Name:**             **omlibdbi**
**Author:**                  `Rainer Gerhards <https://rainer.gerhards.net/>`_ <rgerhards@adiscon.com>
===========================  ===========================================================================


Purpose
=======

This modules supports a large number of database systems via
`libdbi <http://libdbi.sourceforge.net/>`_. Libdbi abstracts the
database layer and provides drivers for many systems. Drivers are
available via the
`libdbi-drivers <http://libdbi-drivers.sourceforge.net/>`_ project. As
of this writing, the following drivers are available:

-  `Firebird/Interbase <http://www.firebird.sourceforge.net/>`_
-  `FreeTDS <http://www.freetds.org/>`_ (provides access to `MS SQL
   Server <http://www.microsoft.com/sql>`_ and
   `Sybase <http://www.sybase.com/products/informationmanagement/adaptiveserverenterprise>`_)
-  `MariaDB <http://www.mariadb.org/>`_/`MySQL <http://www.mysql.com/>`_ 
    (also supported via the `ommysql <ommysql.html>`_ plugin in rsyslog)
-  `PostgreSQL <http://www.postgresql.org/>`_\ (also supported via the
   native `ommysql <ommysql.html>`_ plugin in rsyslog)
-  `SQLite/SQLite3 <http://www.sqlite.org/>`_

The following drivers are in various stages of completion:

-  `Ingres <http://ingres.com/>`_
-  `mSQL <http://www.hughes.com.au/>`_
-  `Oracle <http://www.oracle.com/>`_

These drivers seem to be quite usable, at least from an rsyslog point of
view.

Libdbi provides a slim layer between rsyslog and the actual database
engine. We have not yet done any performance testing (e.g. omlibdbi vs.
:doc:`ommysql`) but honestly believe that the performance impact should be
irrelevant, if at all measurable. Part of that assumption is that
rsyslog just does the "insert" and most of the time is spent either in
the database engine or rsyslog itself. It's hard to think of any
considerable time spent in the libdbi abstraction layer.


Setup
=====

In order for this plugin to work, you need to have libdbi, the libdbi
driver for your database backend and the client software for your
database backend installed. There are libdbi packages for many
distributions. Please note that rsyslogd requires a quite recent version
(0.8.3) of libdbi. It may work with older versions, but these need some
special ./configure options to support being called from a dlopen()ed
plugin (as omlibdbi is). So in short, you probably save you a lot of
headache if you make sure you have at least libdbi version 0.8.3 on your
system.


Configuration Parameters
========================

.. note::

   Parameter names are case-insensitive; camelCase is recommended for readability.


Module Parameters
-----------------

.. list-table::
   :widths: 30 70
   :header-rows: 1

   * - Parameter
     - Summary
   * - :ref:`param-omlibdbi-driverdirectory`
     - .. include:: ../../reference/parameters/omlibdbi-driverdirectory.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omlibdbi-template`
     - .. include:: ../../reference/parameters/omlibdbi-template.rst
        :start-after: .. summary-start
        :end-before: .. summary-end


Input Parameters
-----------------

.. list-table::
   :widths: 30 70
   :header-rows: 1

   * - Parameter
     - Summary
   * - :ref:`param-omlibdbi-driver`
     - .. include:: ../../reference/parameters/omlibdbi-driver.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omlibdbi-server`
     - .. include:: ../../reference/parameters/omlibdbi-server.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omlibdbi-uid`
     - .. include:: ../../reference/parameters/omlibdbi-uid.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omlibdbi-pwd`
     - .. include:: ../../reference/parameters/omlibdbi-pwd.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omlibdbi-db`
     - .. include:: ../../reference/parameters/omlibdbi-db.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omlibdbi-template`
     - .. include:: ../../reference/parameters/omlibdbi-template.rst
        :start-after: .. summary-start
        :end-before: .. summary-end

.. toctree::
   :hidden:

   ../../reference/parameters/omlibdbi-driverdirectory
   ../../reference/parameters/omlibdbi-template
   ../../reference/parameters/omlibdbi-driver
   ../../reference/parameters/omlibdbi-server
   ../../reference/parameters/omlibdbi-uid
   ../../reference/parameters/omlibdbi-pwd
   ../../reference/parameters/omlibdbi-db


Caveats/Known Bugs:
===================

You must make sure that any templates used for omlibdbi properly escape
strings. This is usually done by supplying the SQL (or STDSQL) option to
the template. Omlibdbi rejects templates without this option for
security reasons. However, omlibdbi does not detect if you used the
right option for your backend. Future versions of rsyslog (with
full expression  support) will provide advanced ways of handling this
situation. So far, you must be careful. The default template provided by
rsyslog is suitable for MariaDB/MySQL, but not necessarily for your 
database backend. Be careful!

If you receive the rsyslog error message "libdbi or libdbi drivers not
present on this system" you may either not have libdbi and its drivers
installed or (very probably) the version is earlier than 0.8.3. In this
case, you need to make sure you have at least 0.8.3 and the libdbi
driver for your database backend present on your system.

I do not have most of the database supported by omlibdbi in my lab. So
it received limited cross-platform tests. If you run into troubles, be
sure the let us know at
`http://www.rsyslog.com <http://www.rsyslog.com>`_.


Examples
========

Example 1
---------

The following sample writes all syslog messages to the database
"syslog_db" on mysqlserver.example.com. The server is MariaDB/MySQL and 
being accessed under the account of "user" with password "pwd".

.. code-block:: none

   module(load="omlibdbi")
   action(type="omlibdbi" driver="mysql" server="mysqlserver.example.com"
                          uid="user" pwd="pwd" db="syslog_db")


