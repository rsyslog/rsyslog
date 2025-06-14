GELF forwarding in rsyslog
==========================

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

::

    template(name="gelf" type="list") {
        constant(value="{\"version\":\"1.1\",")
        constant(value="\"host\":\"")
        property(name="hostname")
        constant(value="\",\"short_message\":\"")
        property(name="msg" format="json")
        constant(value="\",\"timestamp\":")
        property(name="timegenerated" dateformat="unixtimestamp")
        constant(value=",\"level\":\"")
        property(name="syslogseverity")
        constant(value="\"}")
    }

This is a typical representation in the list format with all the necessary
fields and format definitions that Graylog expects.

Applying the template to a syslog action
----------------------------------------

The next step is applying the template to our output action. Since we
are forwarding log messages to Graylog, this is usually a syslog sending
action.

::

    # syslog forwarder via UDP
    action(type="omfwd" target="graylogserver" port="12201" protocol="udp" template="gelf")

We now have a syslog forwarding action. This uses the omfwd module. Please
note that the case above only works for UDP transport. When using TCP, 
Graylog expects a Nullbyte as message delimiter. So, to use TCP, you need to change delimiter via `TCP_FrameDelimiter <../configuration/modules/omfwd.html#tcp-framedelimiter>`_ option.

::

    # syslog forwarder via TCP
    action(type="omfwd" target="graylogserver" port="12201" protocol="tcp" template="gelf" TCP_FrameDelimiter="0" KeepAlive="on")

Conclusion
----------

With this quick and easy setup you can feed Graylog with the correct
log message format so it can do its work. This case can be applied to
a lot of different scenarios as well, but with different templates.

This documentation is part of the `rsyslog <http://www.rsyslog.com/>`_
project.


