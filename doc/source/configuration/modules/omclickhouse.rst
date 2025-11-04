**************************************
omclickhouse: ClickHouse Output Module
**************************************

===========================  ===========================================================================
**Module Name:**             **omclickhouse**
**Author:**                  Pascal Withopf <pwithopf@adiscon.com>
===========================  ===========================================================================


Purpose
=======

This module provides native support for logging to
`ClickHouse <https://clickhouse.yandex/>`_.
To enable the module use "--enable-clickhouse" while configuring rsyslog.
Tests for the testbench can be enabled with "--enable-clickhouse-tests".


Notable Features
================

- :ref:`omclickhouse-statistic-counter`


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
   * - :ref:`param-omclickhouse-server`
     - .. include:: ../../reference/parameters/omclickhouse-server.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omclickhouse-port`
     - .. include:: ../../reference/parameters/omclickhouse-port.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omclickhouse-usehttps`
     - .. include:: ../../reference/parameters/omclickhouse-usehttps.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omclickhouse-template`
     - .. include:: ../../reference/parameters/omclickhouse-template.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omclickhouse-bulkmode`
     - .. include:: ../../reference/parameters/omclickhouse-bulkmode.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omclickhouse-maxbytes`
     - .. include:: ../../reference/parameters/omclickhouse-maxbytes.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omclickhouse-user`
     - .. include:: ../../reference/parameters/omclickhouse-user.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omclickhouse-pwd`
     - .. include:: ../../reference/parameters/omclickhouse-pwd.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omclickhouse-errorfile`
     - .. include:: ../../reference/parameters/omclickhouse-errorfile.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omclickhouse-allowunsignedcerts`
     - .. include:: ../../reference/parameters/omclickhouse-allowunsignedcerts.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omclickhouse-skipverifyhost`
     - .. include:: ../../reference/parameters/omclickhouse-skipverifyhost.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omclickhouse-healthchecktimeout`
     - .. include:: ../../reference/parameters/omclickhouse-healthchecktimeout.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omclickhouse-timeout`
     - .. include:: ../../reference/parameters/omclickhouse-timeout.rst
        :start-after: .. summary-start
        :end-before: .. summary-end

.. toctree::
   :hidden:

   ../../reference/parameters/omclickhouse-server
   ../../reference/parameters/omclickhouse-port
   ../../reference/parameters/omclickhouse-usehttps
   ../../reference/parameters/omclickhouse-template
   ../../reference/parameters/omclickhouse-bulkmode
   ../../reference/parameters/omclickhouse-maxbytes
   ../../reference/parameters/omclickhouse-user
   ../../reference/parameters/omclickhouse-pwd
   ../../reference/parameters/omclickhouse-errorfile
   ../../reference/parameters/omclickhouse-allowunsignedcerts
   ../../reference/parameters/omclickhouse-skipverifyhost
   ../../reference/parameters/omclickhouse-healthchecktimeout
   ../../reference/parameters/omclickhouse-timeout


.. _omclickhouse-statistic-counter:

Statistic Counter
=================

This plugin maintains global :doc:`statistics <../rsyslog_statistic_counter>`,
which accumulate all action instances. The statistic is named "omclickhouse".
Parameters are:

-  **submitted** - number of messages submitted for processing (with both
   success and error result)

-  **fail.httprequests** - the number of times an HTTP request failed. Note
   that a single HTTP request may be used to submit multiple messages, so this
   number may be (much) lower than failed.http.

-  **failed.http** - number of message failures due to connection like-problems
   (things like remote server down, broken link etc)

-  **fail.clickhouse** - number of failures due to clickhouse error reply; Note that
   this counter does NOT count the number of failed messages but the number of
   times a failure occurred (a potentially much smaller number). Counting messages
   would be quite performance-intense and is thus not done.

-  **response.success** - number of records successfully sent in bulk index
   requests - counts the number of successful responses


**The fail.httprequests and failed.http counters reflect only failures that
omclickhouse detected.** Once it detects problems, it (usually, depends on
circumstances) tell the rsyslog core that it wants to be suspended until the
situation clears (this is a requirement for rsyslog output modules). Once it is
suspended, it does NOT receive any further messages. Depending on the user
configuration, messages will be lost during this period. Those lost messages will
NOT be counted by impstats (as it does not see them).


Examples
========

Example 1
---------

The following sample does the following:

-  loads the omclickhouse module
-  outputs all logs to ClickHouse using the default settings

.. code-block:: none

    module(load="omclickhouse")
    action(type="omclickhouse")


Example 2
---------

In this example the URL will use `http` and the specified parameters to create
the REST URL.

.. code-block:: none

   module(load="omclickhouse")
   action(type="omclickhouse" server="127.0.0.1" port="8124" user="user1" pwd="pwd1"
          usehttps="off")


Example 3
---------

This example will send messages in batches up to 10MB.
If an error occurs it will be written in the error file.

.. code-block:: none

   module(load="omclickhouse")
   action(type="omclickhouse" maxbytes="10mb" errorfile="clickhouse-error.log")


