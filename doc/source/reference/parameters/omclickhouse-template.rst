.. _param-omclickhouse-template:
.. _omclickhouse.parameter.input.template:

template
========

.. index::
   single: omclickhouse; template
   single: template

.. summary-start

Selects the message template that renders the INSERT statement sent to
ClickHouse.

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
This is the message format that will be sent to ClickHouse. The resulting
string needs to be a valid INSERT Query, otherwise ClickHouse will return an
error. Custom INSERT templates must enable the template-level SQL escaping
option, for example ``option.stdsql="on"`` in RainerScript. ``stdSQL`` is not
an ``omclickhouse`` action parameter. Defaults to:

.. note::

   The leading space in `` StdClickHouseFmt`` is intentional. Rsyslog
   registers its built-in templates with a leading space in the internal
   configuration state, and the module looks up the default by that exact
   name. When overriding the parameter yourself, use the natural form
   ``StdClickHouseFmt`` (without the space) as shown below.

.. code-block:: none

   "\"INSERT INTO rsyslog.SystemEvents (severity, facility, "
   "timestamp, hostname, tag, message) VALUES ("
   "%syslogseverity%, %syslogfacility%, "
   "'%timereported:::date-unixtimestamp%', '%hostname%', "
   "'%syslogtag%', '%msg%')\""

Input usage
-----------
.. _omclickhouse.parameter.input.template-usage:

.. code-block:: rsyslog

   module(load="omclickhouse")
   template(name="CustomClickHouseFmt" type="string" option.stdsql="on"
            string="INSERT INTO rsyslog.SystemEvents (severity, facility, timestamp, hostname, tag, message) VALUES (%syslogseverity%, %syslogfacility%, '%timereported:::date-unixtimestamp%', '%hostname%', '%syslogtag%', '%msg%')")
   action(type="omclickhouse" template="CustomClickHouseFmt")

See also
--------
See also :doc:`/configuration/modules/omclickhouse`.
