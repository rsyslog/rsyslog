*******************************************
pmrfc3164: Parse RFC3164-formatted messages
*******************************************

===========================  ===========================================================================
**Module Name:**             **pmrfc3164**
**Author:**                  `Rainer Gerhards <https://rainer.gerhards.net/>`_ <rgerhards@adiscon.com>
===========================  ===========================================================================


Purpose
=======

This parser module is for parsing messages according to the traditional/legacy
syslog standard :rfc:`3164`

It is part of the default parser chain.

The parser can also be customized to allow the parsing of specific formats,
if they occur.


Configuration Parameters
========================

.. note::

   Parameter names are case-insensitive.


Parser Parameters
-----------------

permit.squareBracketsInHostname
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "off", "no", "none"

This setting tells the parser that hostnames that are enclosed by brackets
should omit the brackets.


permit.slashesInHostname
^^^^^^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "off", "no", "none"

.. versionadded:: 8.20.0

This setting tells the parser that hostnames may contain slashes. This
is useful when messages e.g. from a syslog-ng relay chain are received.
Syslog-ng puts the various relay hosts via slashes into the hostname
field.


permit.AtSignsInHostname
^^^^^^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "off", "no", "none"

.. versionadded:: 8.25.0

This setting tells the parser that hostnames may contain at-signs. This
is useful when messages are relayed from a syslog-ng server in rfc3164
format. The hostname field sent by syslog-ng may be prefixed by the source
name followed by an at-sign character.


force.tagEndingByColon
^^^^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "off", "no", "none"

.. versionadded:: 8.25.0

This setting tells the parser that tag need to be ending by colon to be
valid.  In others case, the tag is set to dash ("-") without changing
message.


remove.msgFirstSpace
^^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "off", "no", "none"

.. versionadded:: 8.25.0

Rfc3164 tell message is directly after tag including first white space.
This option tell to remove the first white space in message just after
reading. It make rfc3164 & rfc5424 syslog messages working in a better way.


detect.YearAfterTimestamp
^^^^^^^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "off", "no", "none"

Some devices send syslog messages in a format that is similar to RFC3164,
but they also attach the year to the timestamp (which is not compliant to
the RFC). With regular parsing, the year would be recognized to be the
hostname and the hostname would become the syslogtag. This setting should
prevent this. It is also limited to years between 2000 and 2099, so
hostnames with numbers as their name can still be recognized correctly. But
everything in this range will be detected as a year.


Examples
========

Receiving malformed RFC3164 messages
------------------------------------

We assume a scenario where some of the devices send malformed RFC3164
messages. The parser module will automatically detect the malformed
sections and parse them accordingly.

.. code-block:: none

   module(load="imtcp")

   input(type="imtcp" port="514" ruleset="customparser")

   parser(name="custom.rfc3164"
   	 type="pmrfc3164"
   	 permit.squareBracketsInHostname="on"
   	 detect.YearAfterTimestamp="on")

   ruleset(name="customparser" parser="custom.rfc3164") {
   	 ... do processing here ...
   }

