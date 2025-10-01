.. _ref-templates-examples:

Template examples
=================

.. summary-start

Practical templates for files, forwarding, databases, JSON output,
and dynamic file names.
.. summary-end

Standard template for writing to files
--------------------------------------------------

.. code-block:: none

    template(name="FileFormat" type="list") {
        property(name="timestamp" dateFormat="rfc3339")
        constant(value=" ")
        property(name="hostname")
        constant(value=" ")
        property(name="syslogtag")
        property(name="msg" spIfNo1stSp="on")
        property(name="msg" dropLastLf="on")
        constant(value="\n")
    }

Equivalent string template:

.. code-block:: none

    template(name="FileFormat" type="string"
             string="%TIMESTAMP% %HOSTNAME% %syslogtag%%msg:::sp-if-no-1st-sp%%msg:::drop-last-lf%\n")

Standard template for forwarding to a remote host (RFC3164)
--------------------------------------------------------------

.. code-block:: none

    template(name="ForwardFormat" type="list") {
        constant(value="<")
        property(name="pri")
        constant(value=">")
        property(name="timestamp" dateFormat="rfc3339")
        constant(value=" ")
        property(name="hostname")
        constant(value=" ")
        property(name="syslogtag" position.from="1" position.to="32")
        property(name="msg" spIfNo1stSp="on")
        property(name="msg")
    }

Equivalent string template:

.. code-block:: none

    template(name="ForwardFormat" type="string"
             string="<%PRI%>%TIMESTAMP:::date-rfc3339% %HOSTNAME% %syslogtag:1:32%%msg:::sp-if-no-1st-sp%%msg%")

Standard template for writing to the MariaDB/MySQL database
--------------------------------------------------------------------------

.. code-block:: none

    template(name="StdSQLformat" type="list" option.sql="on") {
        constant(value="insert into SystemEvents (Message, Facility, FromHost, Priority, DeviceReportedTime, ReceivedAt, InfoUnitID, SysLogTag)")
        constant(value=" values ('")
        property(name="msg")
        constant(value="', ")
        property(name="syslogfacility")
        constant(value=", '")
        property(name="hostname")
        constant(value="', ")
        property(name="syslogpriority")
        constant(value=", '")
        property(name="timereported" dateFormat="mysql")
        constant(value="', '")
        property(name="timegenerated" dateFormat="mysql")
        constant(value="', ")
        property(name="iut")
        constant(value=", '")
        property(name="syslogtag")
        constant(value="')")
    }

Equivalent string template:

.. code-block:: none

    template(name="StdSQLformat" type="string" option.sql="on"
             string="insert into SystemEvents (Message, Facility, FromHost, Priority, DeviceReportedTime, ReceivedAt, InfoUnitID, SysLogTag) values ('%msg%', %syslogfacility%, '%HOSTNAME%', %syslogpriority%, '%timereported:::date-mysql%', '%timegenerated:::date-mysql%', %iut%, '%syslogtag%')")

Generating JSON
---------------

Useful for RESTful APIs such as ElasticSearch.

.. code-block:: none

    template(name="outfmt" type="list" option.jsonf="on") {
        property(outname="@timestamp" name="timereported" dateFormat="rfc3339" format="jsonf")
        property(outname="host" name="hostname" format="jsonf")
        property(outname="severity" name="syslogseverity" caseConversion="upper" format="jsonf" dataType="number")
        property(outname="facility" name="syslogfacility" format="jsonf" dataType="number")
        property(outname="syslog-tag" name="syslogtag" format="jsonf")
        property(outname="source" name="app-name" format="jsonf" onEmpty="null")
        property(outname="message" name="msg" format="jsonf")
    }

Produces output similar to:

.. code-block:: none

    {"@timestamp":"2018-03-01T01:00:00+00:00", "host":"192.0.2.8", "severity":7, "facility":20, "syslog-tag":"tag", "source":"tag", "message":" msgnum:00000000:"}

Pretty-printed:

.. code-block:: none

    {
      "@timestamp": "2018-03-01T01:00:00+00:00",
      "host": "192.0.2.8",
      "severity": 7,
      "facility": 20,
      "syslog-tag": "tag",
      "source": "tag",
      "message": " msgnum:00000000:"
    }

If ``onEmpty="null"`` is used and ``source`` is empty:

.. code-block:: none

    {"@timestamp":"2018-03-01T01:00:00+00:00", "host":"192.0.2.8", "severity":7, "facility":20, "syslog-tag":"tag", "source":null, "message":" msgnum:00000000:"}

.. note:: The output is not pretty-printed in actual use to avoid waste of resources.

Creating dynamic file names for ``omfile``
---------------------------------------------------------

Templates can generate dynamic file names. For example, to split syslog
messages by host name:

.. code-block:: none

   template(name="DynFile" type="string" string="/var/log/system-%HOSTNAME%.log")

