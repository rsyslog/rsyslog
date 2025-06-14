*********************************
pmnull: Syslog Null Parser Module
*********************************

===========================  ===========================================================================
**Module Name:**             **pmnull**
**Author:**                  `Rainer Gerhards <https://rainer.gerhards.net/>`_ <rgerhards@adiscon.com>
===========================  ===========================================================================


Purpose
=======

When a message is received it is tried to match a set of parsers to get
properties populated. This parser module sets all attributes to "" but rawmsg.
There usually should be no need to use this module. It may be useful to
process certain known-non-syslog messages.

The pmnull module was originally written as some people thought it would
be nice to save 0.05% of  time by not unnecessarily parsing the message.
We even doubt it is that amount of performance enhancement as the properties
need to be populated in any case, so the saving is really minimal (but exists).

**If you just want to transmit or store messages exactly in the format that
they arrived in you do not need pmnull!** You can use the `rawmsg` property::

	template(name="asReceived" type="string" string="%rawmsg%")
	action(type="omfwd" target="server.example.net" template="asReceived")

Configuration Parameters
========================

.. note::

   Parameter names are case-insensitive.


Parser Parameters
-----------------

Tag
^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "string", "", "no", "none"

This setting sets the tag value to the message.


SyslogFacility
^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "Facility", "1", "no", "none"

This setting sets the syslog facility value. The default comes from the
rfc3164 standard.


SyslogSeverity
^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "Severity", "5", "no", "none"

This setting sets the syslog severity value. The default comes from the
rfc3164 standard.


Examples
========

Process messages received via imtcp
-----------------------------------

In this example messages are received through imtcp on port 13514. The
ruleset uses the parser pmnull which has the parameters tag, syslogfacility
and syslogseverity given.

.. code-block:: none

   module(load="imtcp")
   module(load="pmnull")

   input(type="imtcp" port="13514" ruleset="ruleset")
   parser(name="custom.pmnull" type="pmnull" tag="mytag" syslogfacility="3"
   	 syslogseverity="1")

   ruleset(name="ruleset" parser=["custom.pmnull", "rsyslog.pmnull"]) {
   	action(type="omfile" file="rsyslog.out.log")
   }


Process messages with default parameters
----------------------------------------

In this example the ruleset uses the parser pmnull with the default parameters
because no specifics were given.

.. code-block:: none

   module(load="imtcp")
   module(load="pmnull")

   input(type="imtcp" port="13514" ruleset="ruleset")
   parser(name="custom.pmnull" type="pmnull")

   ruleset(name="ruleset" parser="custom.pmnull") {
   	action(type="omfile" file="rsyslog.out.log")
   }

