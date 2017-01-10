pmnull: Syslog Null Parser Module
=================================

**Author**: Pascal Withopf <pascalwithopf1@gmail.com>

When a message is received it is tried to match a set of parsers to get
properties populated. This parser module sets all attributes to "" but rawmsg.
It is used as a performance improvment since there are no CPU cycles wasted on
parsing the message.

Parser Parameters
-----------------

.. function:: tag <string>

   **Default**: Empty ("")

   This setting sets the tag value to the message.

.. function:: syslogfacility <int>

   **Default**: 1

   This setting sets the syslog facility value. The default comes from the
   rfc3164 standard.

.. function:: syslogseverity <int>

   **Default**: 5

   This setting sets the syslog severity value. The default comes from the
   rfc3164 standard.

Example
-------
In this example messages are received through imtcp on port 13514. The
ruleset uses the parser pmnull which has the parameters tag, syslogfacility
and syslogseverity given.

:: 
   module(load="imtcp")
   module(load="pmnull")
   input(type="imtcp" port="13514" ruleset="ruleset")
   parser(name="custom.pmnull" type="pmnull" tag="mytag" syslogfacility="3"
   syslogseverity="1")
   ruleset(name="ruleset" parser=["custom.pmnull", "rsyslog.pmnull"]) {
   action(type="omfile" file="rsyslog.out.log")
   }


In this example the ruleset uses the parser pmnull with the default parameters
because no specifics were given.

::
   module(load="imtcp")
   module(load="pmnull")
   input(type="imtcp" port="13514" ruleset="ruleset")
   parser(name="custom.pmnull" type="pmnull")
   ruleset(name="ruleset" parser="custom.pmnull") {
   action(type="omfile" file="rsyslog.out.log")
   }
