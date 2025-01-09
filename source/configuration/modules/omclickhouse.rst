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

   Parameter names are case-insensitive.


Action Parameters
-----------------

Server
^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "localhost", "no", "none"

The address of a ClickHouse server.

.. _port:

Port
^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "8123", "no", "none"

HTTP port to use to connect to ClickHouse.


usehttps
^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "on", "no", "none"

Default scheme to use when sending events to ClickHouse if none is
specified on a server.


template
^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "StdClickHouseFmt", "no", "none"

This is the message format that will be sent to ClickHouse. The
resulting string needs to be a valid INSERT Query, otherwise ClickHouse
will return an error. Defaults to:

.. code-block:: none

    "\"INSERT INTO rsyslog.SystemEvents (severity, facility, "
    "timestamp, hostname, tag, message) VALUES (%syslogseverity%, %syslogfacility%, "
    "'%timereported:::date-unixtimestamp%', '%hostname%', '%syslogtag%', '%msg%')\""


bulkmode
^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "on", "no", "none"

The "off" setting means logs are shipped one by one. Each in
its own HTTP request.
The default "on" will send multiple logs in the same request. This is
recommended, because it is many times faster than when bulkmode is turned off.
The maximum number of logs sent in a single bulk request depends on your
maxbytes and queue settings - usually limited by the `dequeue batch
size <http://www.rsyslog.com/doc/node35.html>`_. More information
about queues can be found
`here <http://www.rsyslog.com/doc/node32.html>`_.


maxbytes
^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "Size", "104857600/100mb", "no", "none"

When shipping logs with bulkmode **on**, maxbytes specifies the maximum
size of the request body sent to ClickHouse. Logs are batched until
either the buffer reaches maxbytes or the `dequeue batch
size <http://www.rsyslog.com/doc/node35.html>`_ is reached.


user
^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "default", "no", "none"

If you have basic HTTP authentication deployed you can specify your user-name here.


pwd
^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "", "no", "none"

Password for basic authentication.


errorFile
^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "none", "no", "none"

If specified, records failed in bulk mode are written to this file, including
their error cause. Rsyslog itself does not process the file any more, but the
idea behind that mechanism is that the user can create a script to periodically
inspect the error file and react appropriately. As the complete request is
included, it is possible to simply resubmit messages from that script.

*Please note:* when rsyslog has problems connecting to clickhouse, a general
error is assumed. However, if we receive negative responses during batch
processing, we assume an error in the data itself (like a mandatory field is
not filled in, a format error or something along those lines). Such errors
cannot be solved by simply resubmitting the record. As such, they are written
to the error file so that the user (script) can examine them and act appropriately.
Note that e.g. after search index reconfiguration (e.g. dropping the mandatory
attribute) a resubmit may be successful.


allowUnsignedCerts
^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "on", "no", "none"

The module accepts connections to servers, which have unsigned certificates.
If this parameter is disabled, the module will verify whether the certificates
are authentic.


skipverifyhost
^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "boolean", "off", "no", "none"

If `"on"`, this will set the curl `CURLOPT_SSL_VERIFYHOST` option to
`0`.  You are strongly discouraged to set this to `"on"`.  It is
primarily useful only for debugging or testing.


healthCheckTimeout
^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "int", "3500", "no", "none"

This parameter sets the timeout for checking the availability
of ClickHouse. Value is given in milliseconds.


timeout
^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "int", "0", "no", "none"

This parameter sets the timeout for sending data to ClickHouse.
Value is given in milliseconds.


.. _omclickhouse-statistic-counter:

Statistic Counter
=================

This plugin maintains global :doc:`statistics <../rsyslog_statistic_counter>`,
which accumulate all action instances. The statistic is named "omclickhouse".
Parameters are:

-  **submitted** - number of messages submitted for processing (with both
   success and error result)

-  **fail.httprequests** - the number of times an http request failed. Note
   that a single http request may be used to submit multiple messages, so this
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

In this example the URL will use http and the specified parameters to create
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


