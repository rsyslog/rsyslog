.. _param-omclickhouse-template:
.. _omclickhouse.parameter.input.template:

template
========

.. index::
   single: omclickhouse; template
   single: template

.. summary-start

Selects the message template that renders the INSERT statement sent to ClickHouse.

.. summary-end

This parameter applies to :doc:`/configuration/modules/omclickhouse`.

:Name: template
:Scope: input
:Type: word
:Default: `` StdClickHouseFmt``
:Required?: no
:Introduced: not specified

Description
-----------
This is the message format that will be sent to ClickHouse. The resulting string needs to be a valid INSERT Query, otherwise ClickHouse will return an error. Defaults to:

.. code-block:: none

   "\"INSERT INTO rsyslog.SystemEvents (severity, facility, "
   "timestamp, hostname, tag, message) VALUES (%syslogseverity%, %syslogfacility%, "
   "'%timereported:::date-unixtimestamp%', '%hostname%', '%syslogtag%', '%msg%')\""

Input usage
-----------
.. _omclickhouse.parameter.input.template-usage:

.. code-block:: rsyslog

   module(load="omclickhouse")
   action(type="omclickhouse" template="StdClickHouseFmt")

See also
--------
See also :doc:`/configuration/modules/omclickhouse`.
