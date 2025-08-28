.. _ref-templates-reserved-names:

Reserved template names
=======================

.. summary-start

Names beginning with ``RSYSLOG_`` are reserved. The following templates
are predefined and may be used without explicit definition.
.. summary-end

Template names starting with ``RSYSLOG_`` are reserved for rsyslog. Do not
create custom templates with this prefix to avoid conflicts. The
following predefined templates are available:

**RSYSLOG_TraditionalFileFormat** – "old style" default log file format
with low-precision timestamps.

.. code-block:: none

   template(name="RSYSLOG_TraditionalFileFormat" type="string"
        string="%TIMESTAMP% %HOSTNAME% %syslogtag%%msg:::sp-if-no-1st-sp%%msg:::drop-last-lf%\n")

**RSYSLOG_FileFormat** – modern logfile format similar to
TraditionalFileFormat with high-precision timestamps and timezone
information.

.. code-block:: none

   template(name="RSYSLOG_FileFormat" type="list") {
        property(name="timereported" dateFormat="rfc3339")
        constant(value=" ")
        property(name="hostname")
        constant(value=" ")
        property(name="syslogtag")
        property(name="msg" spIfNo1stSp="on")
        property(name="msg" dropLastLf="on")
        constant(value="\n")
   }

**RSYSLOG_TraditionalForwardFormat** – traditional forwarding format with
low-precision timestamps. Useful when sending to older syslogd instances.

.. code-block:: none

   template(name="RSYSLOG_TraditionalForwardFormat" type="string"
        string="<%PRI%>%TIMESTAMP% %HOSTNAME% %syslogtag:1:32%%msg:::sp-if-no-1st-sp%%msg%")

**RSYSLOG_SysklogdFileFormat** – sysklogd compatible log file format. Use
with options ``$SpaceLFOnReceive on``, ``$EscapeControlCharactersOnReceive off``
and ``$DropTrailingLFOnReception off`` for full compatibility.

.. code-block:: none

   template(name="RSYSLOG_SysklogdFileFormat" type="string"
        string="%TIMESTAMP% %HOSTNAME% %syslogtag%%msg:::sp-if-no-1st-sp%%msg%\n")

**RSYSLOG_ForwardFormat** – high-precision forwarding format similar to
the traditional one but with high-precision timestamps and timezone
information.

.. code-block:: none

   template(name="RSYSLOG_ForwardFormat" type="string"
        string="<%PRI%>%TIMESTAMP:::date-rfc3339% %HOSTNAME% %syslogtag:1:32%%msg:::sp-if-no-1st-sp%%msg%")

**RSYSLOG_SyslogProtocol23Format** – format from
IETF draft *ietf-syslog-protocol-23*, close to `RFC5424
<https://datatracker.ietf.org/doc/html/rfc5424>`_.

.. code-block:: none

   template(name="RSYSLOG_SyslogProtocol23Format" type="string"
        string="<%PRI%>1 %TIMESTAMP:::date-rfc3339% %HOSTNAME% %APP-NAME% %PROCID% %MSGID% %STRUCTURED-DATA% %msg%\n")

**RSYSLOG_DebugFormat** – used for troubleshooting property problems.
Write to a log file only; do **not** use for production or forwarding.

.. code-block:: none

   template(name="RSYSLOG_DebugFormat" type="list") {
        constant(value="Debug line with all properties:\nFROMHOST: '")
        property(name="fromhost")
        constant(value="', fromhost-ip: '")
        property(name="fromhost-ip")
        constant(value="', HOSTNAME: '")
        property(name="hostname")
        constant(value="', PRI: '")
        property(name="pri")
        constant(value=",\nsyslogtag '")
        property(name="syslogtag")
        constant(value="', programname: '")
        property(name="programname")
        constant(value="', APP-NAME: '")
        property(name="app-name")
        constant(value="', PROCID: '")
        property(name="procid")
        constant(value="', MSGID: '")
        property(name="msgid")
        constant(value="',\nTIMESTAMP: '")
        property(name="timereported")
        constant(value="', STRUCTURED-DATA: '")
        property(name="structured-data")
        constant(value="',\nmsg: '")
        property(name="msg")
        constant(value="'\nescaped msg: '")
        property(name="msg" controlCharacters="drop")
        constant(value="'\ninputname: ")
        property(name="inputname")
        constant(value=" rawmsg: '")
        property(name="rawmsg")
        constant(value="'\n$!:")
        property(name="$!")
        constant(value="\n$.:")
        property(name="$.")
        constant(value="\n$/:")
        property(name="$/")
        constant(value="\n\n")
   }

**RSYSLOG_WallFmt** – information about host and time followed by the
syslogtag and message.

.. code-block:: none

   template(name="RSYSLOG_WallFmt" type="string"
        string="\r\n\7Message from syslogd@%HOSTNAME% at %timegenerated% ...\r\n%syslogtag%%msg%\n\r")

**RSYSLOG_StdUsrMsgFmt** – syslogtag followed by the message.

.. code-block:: none

   template(name="RSYSLOG_StdUsrMsgFmt" type="string"
        string=" %syslogtag%%msg%\n\r")

**RSYSLOG_StdDBFmt** – insert command for table ``SystemEvents`` in
MariaDB/MySQL.

.. code-block:: none

   template(name="RSYSLOG_StdDBFmt" type="list") {
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
        property(name="timereported" dateFormat="date-mysql")
        constant(value="', '")
        property(name="timegenerated" dateFormat="date-mysql")
        constant(value="', ")
        property(name="iut")
        constant(value=", '")
        property(name="syslogtag")
        constant(value="')")
   }

**RSYSLOG_StdPgSQLFmt** – insert command for table ``SystemEvents`` in
pgsql.

.. code-block:: none

   template(name="RSYSLOG_StdPgSQLFmt" type="string"
   string="insert into SystemEvents (Message, Facility, FromHost, Priority, DeviceReportedTime,
        ReceivedAt, InfoUnitID, SysLogTag) values ('%msg%', %syslogfacility%, '%HOSTNAME%',
        %syslogpriority%, '%timereported:::date-pgsql%', '%timegenerated:::date-pgsql%', %iut%,
        '%syslogtag%')")

**RSYSLOG_spoofadr** – message containing only the sender's IP address.

.. code-block:: none

   template(name="RSYSLOG_spoofadr" type="string" string="%fromhost-ip%")

**RSYSLOG_StdJSONFmt** – JSON structure containing message properties.

.. code-block:: none

   template(name="RSYSLOG_StdJSONFmt" type="string"
        string="{\"message\":\"%msg:::json%\",\"fromhost\":\"%HOSTNAME:::json%\",\"facility\":\"%syslogfacility-text%\",\"priority\":\"%syslogpriority-text%\",\"timereported\":\"%timereported:::date-rfc3339%\",\"timegenerated\":\"%timegenerated:::date-rfc3339%\"}")

