rsyslog Properties
==================

Data items in rsyslog are called "properties". They can have different
origin. The most important ones are those that stem from received
messages. But there are also others. Whenever you want to access data items,
you need to access the respective property.

Properties are used in

- :doc:`templates <templates>`
- conditional statements

The property name is case-insensitive (prior to 3.17.0, they were case-sensitive).

Note: many users refer to "rsyslog properties" as "rsyslog variables". You can treat
them as synonymous.
Read how `rsyslog lead author Rainer Gerhards explains the naming
difference <https://rainer.gerhards.net/2020/08/rsyslog-template-variables-where-to-find-them.html">`_.

Message Properties
------------------
These are extracted by rsyslog parsers from the original message. All message
properties start with a letter.

The following message properties exist:

.. note::

   Property names are case-insensitive. Use the spelling from headings in prose and examples.

.. list-table::
   :widths: 30 70
   :header-rows: 1

   * - Property
     - Summary
   * - :ref:`prop-message-msg`
     - .. include:: ../reference/properties/message-msg.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`prop-message-rawmsg`
     - .. include:: ../reference/properties/message-rawmsg.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`prop-message-rawmsg-after-pri`
     - .. include:: ../reference/properties/message-rawmsg-after-pri.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`prop-message-hostname`
     - .. include:: ../reference/properties/message-hostname.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`prop-message-source`
     - .. include:: ../reference/properties/message-source.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`prop-message-fromhost`
     - .. include:: ../reference/properties/message-fromhost.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`prop-message-fromhost-ip`
     - .. include:: ../reference/properties/message-fromhost-ip.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`prop-message-fromhost-port`
     - .. include:: ../reference/properties/message-fromhost-port.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`prop-message-syslogtag`
     - .. include:: ../reference/properties/message-syslogtag.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`prop-message-programname`
     - .. include:: ../reference/properties/message-programname.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`prop-message-pri`
     - .. include:: ../reference/properties/message-pri.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`prop-message-pri-text`
     - .. include:: ../reference/properties/message-pri-text.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`prop-message-iut`
     - .. include:: ../reference/properties/message-iut.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`prop-message-syslogfacility`
     - .. include:: ../reference/properties/message-syslogfacility.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`prop-message-syslogfacility-text`
     - .. include:: ../reference/properties/message-syslogfacility-text.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`prop-message-syslogseverity`
     - .. include:: ../reference/properties/message-syslogseverity.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`prop-message-syslogseverity-text`
     - .. include:: ../reference/properties/message-syslogseverity-text.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`prop-message-syslogpriority`
     - .. include:: ../reference/properties/message-syslogpriority.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`prop-message-syslogpriority-text`
     - .. include:: ../reference/properties/message-syslogpriority-text.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`prop-message-timegenerated`
     - .. include:: ../reference/properties/message-timegenerated.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`prop-message-timereported`
     - .. include:: ../reference/properties/message-timereported.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`prop-message-timestamp`
     - .. include:: ../reference/properties/message-timestamp.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`prop-message-protocol-version`
     - .. include:: ../reference/properties/message-protocol-version.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`prop-message-structured-data`
     - .. include:: ../reference/properties/message-structured-data.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`prop-message-app-name`
     - .. include:: ../reference/properties/message-app-name.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`prop-message-procid`
     - .. include:: ../reference/properties/message-procid.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`prop-message-msgid`
     - .. include:: ../reference/properties/message-msgid.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`prop-message-inputname`
     - .. include:: ../reference/properties/message-inputname.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`prop-message-uuid`
     - .. include:: ../reference/properties/message-uuid.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`prop-message-jsonmesg`
     - .. include:: ../reference/properties/message-jsonmesg.rst
        :start-after: .. summary-start
        :end-before: .. summary-end

System Properties
-----------------
These properties are provided by the rsyslog core engine. They are **not**
related to the message. All system properties start with a dollar-sign.

Special care needs to be taken in regard to time-related system variables:

* ``timereported`` contains the timestamp that is contained within the
  message header. Ideally, it resembles the time when the message was
  created at the original sender.
  Depending on how long the message was in the relay chain, this
  can be quite old.
* ``timegenerated`` contains the timestamp when the message was received
  by the local system. Here "received" actually means the point in time
  when the message was handed over from the OS to rsyslog's reception
  buffers, but before any actual processing takes place. This also means
  a message is "received" before it is placed into any queue. Note that
  depending on the input, some minimal processing like extraction of the
  actual message content from the receive buffer can happen. If multiple
  messages are received via the same receive buffer (a common scenario
  for example with TCP-based syslog), they bear the same ``timegenerated``
  stamp because they actually were received at the same time.
* ``$now`` is **not** from the message. It is the system time when the
  message is being **processed**. There is always a small difference
  between ``timegenerated`` and ``$now`` because processing always
  happens after reception. If the message is sitting inside a queue
  on the local system, the time difference between the two can be some
  seconds (e.g. due to a message burst and in-memory queueing) up to
  several hours in extreme cases where a message is sitting inside a
  disk queue (e.g. due to a database outage). The ``timereported``
  property is usually older than ``timegenerated``, but may be totally
  different due to differences in time and time zone configuration
  between systems.

The following system properties exist:

.. list-table::
   :widths: 30 70
   :header-rows: 1

   * - Property
     - Summary
   * - :ref:`prop-system-bom`
     - .. include:: ../reference/properties/system-bom.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`prop-system-myhostname`
     - .. include:: ../reference/properties/system-myhostname.rst
        :start-after: .. summary-start
        :end-before: .. summary-end

Time-Related System Properties
..............................

All of these system properties exist in a local time variant (e.g. \$now)
and a variant that emits UTC (e.g. \$now-utc). The UTC variant is always
available by appending "-utc". Note that within a single template, only
the localtime or UTC variant should be used. While it is possible to mix
both variants within a single template, it is **not** guaranteed that
they will provide exactly the same time. The technical reason is that
rsyslog needs to re-query system time when the variant is changed. Because
of this, we strongly recommend not mixing both variants in the same
template.

Note that use in different templates will generate a consistent timestamp
within each template. However, as $now always provides local system time
at time of using it, time may advance and consequently different templates
may have different time stamp. To avoid this, use *timegenerated* instead.

.. list-table::
   :widths: 30 70
   :header-rows: 1

   * - Property
     - Summary
   * - :ref:`prop-system-time-now`
     - .. include:: ../reference/properties/system-time-now.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`prop-system-time-year`
     - .. include:: ../reference/properties/system-time-year.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`prop-system-time-month`
     - .. include:: ../reference/properties/system-time-month.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`prop-system-time-day`
     - .. include:: ../reference/properties/system-time-day.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`prop-system-time-wday`
     - .. include:: ../reference/properties/system-time-wday.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`prop-system-time-hour`
     - .. include:: ../reference/properties/system-time-hour.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`prop-system-time-hhour`
     - .. include:: ../reference/properties/system-time-hhour.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`prop-system-time-qhour`
     - .. include:: ../reference/properties/system-time-qhour.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`prop-system-time-minute`
     - .. include:: ../reference/properties/system-time-minute.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`prop-system-time-now-unixtimestamp`
     - .. include:: ../reference/properties/system-time-now-unixtimestamp.rst
        :start-after: .. summary-start
        :end-before: .. summary-end

.. toctree::
   :hidden:

   ../reference/properties/message-msg
   ../reference/properties/message-rawmsg
   ../reference/properties/message-rawmsg-after-pri
   ../reference/properties/message-hostname
   ../reference/properties/message-source
   ../reference/properties/message-fromhost
   ../reference/properties/message-fromhost-ip
   ../reference/properties/message-fromhost-port
   ../reference/properties/message-syslogtag
   ../reference/properties/message-programname
   ../reference/properties/message-pri
   ../reference/properties/message-pri-text
   ../reference/properties/message-iut
   ../reference/properties/message-syslogfacility
   ../reference/properties/message-syslogfacility-text
   ../reference/properties/message-syslogseverity
   ../reference/properties/message-syslogseverity-text
   ../reference/properties/message-syslogpriority
   ../reference/properties/message-syslogpriority-text
   ../reference/properties/message-timegenerated
   ../reference/properties/message-timereported
   ../reference/properties/message-timestamp
   ../reference/properties/message-protocol-version
   ../reference/properties/message-structured-data
   ../reference/properties/message-app-name
   ../reference/properties/message-procid
   ../reference/properties/message-msgid
   ../reference/properties/message-inputname
   ../reference/properties/message-uuid
   ../reference/properties/message-jsonmesg
   ../reference/properties/system-bom
   ../reference/properties/system-myhostname
   ../reference/properties/system-time-now
   ../reference/properties/system-time-year
   ../reference/properties/system-time-month
   ../reference/properties/system-time-day
   ../reference/properties/system-time-wday
   ../reference/properties/system-time-hour
   ../reference/properties/system-time-hhour
   ../reference/properties/system-time-qhour
   ../reference/properties/system-time-minute
   ../reference/properties/system-time-now-unixtimestamp
