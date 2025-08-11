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


detect.headerless
^^^^^^^^^^^^^^^^^

.. meta::
   :tag: parameter:detect.headerless
   :tag: category:advanced
   :tag: keywords: pmrfc3164, syslog, headerless, detection

Enables extended detection of messages that lack a standard syslog header.

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "off", "no", "none"

.. note::
   Messages beginning with ``{`` or ``[`` are treated as structured syslog
   (e.g. JSON) and **not** classified as headerless.

When set to ``on``, a message is classified as “headerless” only
if **all** of the following are true:

- It has **no** PRI header (facility/severity).
- It does **not** start with an accepted timestamp.
- It does **not** begin with whitespace followed by ``{`` or ``[``.

Related Parameters
------------------
Other action parameters prefixed with ``headerless.`` can be used to further customize
processing of messages identified as headerless (for example, writing them to an
"error file").

headerless.hostname
^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "string", "<ip-address of sender>", "no", "none"

By default, rsyslog uses the IP address from the system it received the
message from as a hostname replacement. This can be overridden by the
hostname configured here.


headerless.tag
^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "string", "headerless", "no", "none"

Specifies the tag to assign to detected headerless messages.


headerless.ruleset
^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "string", "none", "no", "none"

If set, detected headerless messages are routed to the given ruleset for
further processing (e.g. writing to a dedicated error log or discarding).

headerless.errorfile
^^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "string", "none", "no", "none"

If specified, raw headerless input will be appended to this file before any
further processing. The file is created if it does not yet exist.

headerless.drop
^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "off", "no", "none"

When set to ``on``, headerless messages are discarded after optional logging to
``headerless.errorfile`` and are not processed by subsequent rules.


Signal Handling
===============

HUP Signal Support
------------------

This parser module supports the HUP signal for log rotation when using the
``headerless.errorfile`` parameter. When rsyslog receives a HUP signal, the
module will:

1. Close the current headerless error file
2. Automatically reopen it on the next write operation

This allows external log rotation tools (like ``logrotate``) to safely rotate
the headerless error file by moving/renaming it and then sending a HUP signal
to rsyslog.

**Example log rotation configuration:**

.. code-block:: none

   /var/log/rsyslog-headerless.log {
       daily
       rotate 7
       compress
       delaycompress
       postrotate
           /bin/kill -HUP `cat /var/run/rsyslogd.pid 2> /dev/null` 2> /dev/null || true
       endscript
   }


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
    ... do processing here...
   }
