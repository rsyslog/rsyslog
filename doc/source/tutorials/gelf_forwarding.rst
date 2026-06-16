GELF forwarding in rsyslog
==========================

.. meta::
   :description: Forward rsyslog messages to Graylog in GELF format.
   :keywords: rsyslog, GELF, Graylog, omfwd, jsonf, template

.. summary-start

Forward rsyslog messages to Graylog by rendering a GELF JSON template and
using ``omfwd`` with UDP or TCP framing.

.. summary-end

*Written by Florian Riedl*

Situation
---------

The current setup has a system with rsyslog as the central syslog server
and a system with Graylog for storage and analyzing the log messages.
Graylog expects the log messages to arrive in GELF (Graylog Extended Log
Format).

Changing the default log format to GELF
---------------------------------------

To make rsyslog send GELF we basically need to create a custom template.
This template will define the format in which the log messages will get
sent to Graylog.

.. code-block:: none

    template(name="gelf" type="list" option.jsonf="on") {
        constant(outname="version" value="1.1" format="jsonf")
        property(outname="host" name="hostname" format="jsonf")
        property(outname="short_message" name="msg" format="jsonf")
        property(outname="timestamp" name="timegenerated"
                 dateformat="unixtimestamp" datatype="number" format="jsonf")
        property(outname="level" name="syslogseverity"
                 datatype="number" format="jsonf")
        property(outname="_app_name" name="app-name" format="jsonf")
        property(outname="_syslog_facility" name="syslogfacility-text"
                 format="jsonf")
    }

This list template uses ``option.jsonf="on"`` and ``format="jsonf"`` so
rsyslog adds JSON braces, commas, field names, and string escaping. The
``timestamp`` and ``level`` fields are emitted as JSON numbers, which avoids
Graylog warnings about string-typed GELF fields. The ``_app_name`` and
``_syslog_facility`` fields are optional GELF additional fields.

Applying the template to a syslog action
----------------------------------------

The next step is applying the template to our output action. Since we
are forwarding log messages to Graylog, this is usually a syslog sending
action.

.. code-block:: none

    # syslog forwarder via UDP
    action(type="omfwd" target="graylogserver" port="12201" protocol="udp" template="gelf")

We now have a syslog forwarding action. This uses the omfwd module. Please
note that the case above only works for UDP transport. When using TCP,
Graylog expects a null byte as message delimiter. To use TCP, configure the
delimiter via the ``TCP_FrameDelimiter`` parameter of the :doc:`omfwd
module <../configuration/modules/omfwd>`.

.. code-block:: none

    # syslog forwarder via TCP
    action(type="omfwd" target="graylogserver" port="12201" protocol="tcp" template="gelf" TCP_FrameDelimiter=0 KeepAlive="on")

Conclusion
----------

With this quick and easy setup you can feed Graylog with the correct
log message format so it can do its work. This case can be applied to
a lot of different scenarios as well, but with different templates.

This documentation is part of the `rsyslog <http://www.rsyslog.com/>`_
project.
