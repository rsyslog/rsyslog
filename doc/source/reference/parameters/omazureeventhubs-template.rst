.. _param-omazureeventhubs-template:
.. _omazureeventhubs.parameter.module.template:

template
========

.. index::
   single: omazureeventhubs; template
   single: template

.. summary-start

Selects the rsyslog template that formats messages sent to Event Hubs.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omazureeventhubs`.

:Name: template
:Scope: module
:Type: word
:Default: module=RSYSLOG_FileFormat
:Required?: no
:Introduced: v8.2304

Description
-----------
Specifies the template used to format and structure the log messages that will
be sent from rsyslog to Microsoft Azure Event Hubs.

The message template can include rsyslog variables, such as the timestamp,
hostname, or process name, and it can use rsyslog macros, such as ``$rawmsg`` or
``$json``, to control the formatting of log data.

For a message template sample with valid JSON output see the sample below:

.. code-block:: none

   template(name="generic" type="list" option.jsonf="on") {
           property(outname="timestamp" name="timereported" dateFormat="rfc3339" format="jsonf")
           constant(value="\"source\": \"EventHubMessage\", ")
           property(outname="host" name="hostname" format="jsonf")
           property(outname="severity" name="syslogseverity" caseConversion="upper" format="jsonf" datatype="number")
           property(outname="facility" name="syslogfacility" format="jsonf" datatype="number")
           property(outname="appname" name="syslogtag" format="jsonf")
           property(outname="message" name="msg" format="jsonf" )
           property(outname="etlsource" name="$myhostname" format="jsonf")
   }

Module usage
------------
.. _param-omazureeventhubs-module-template:
.. _omazureeventhubs.parameter.module.template-usage:

.. code-block:: rsyslog

   action(type="omazureeventhubs" template="generic" ...)

See also
--------
See also :doc:`../../configuration/modules/omazureeventhubs`.
