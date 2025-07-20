*********************************
omjournal: Systemd Journal Output
*********************************

===========================  ===========================================================================
**Module Name:**             **omjournal**
**Author:**                  `Rainer Gerhards <https://rainer.gerhards.net/>`_ <rgerhards@adiscon.com>
===========================  ===========================================================================


Purpose
=======

This module provides native support for logging to the systemd journal.


Configuration Parameters
========================

.. note::

   Parameter names are case-insensitive.


Action Parameters
-----------------

Template
^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "none", "no", "none"

Template to use when submitting messages.

By default, rsyslog will use the incoming %msg% as the MESSAGE field
of the journald entry, and include the syslog tag and priority.

You can override the default formatting of the message, and include
custom fields with a template. Complex fields in the template
(eg. json entries) will be added to the journal as json text. Other
fields will be coerced to strings.

Journald requires that you include a template parameter named MESSAGE.


Examples
========

Example 1
---------

The following sample writes all syslog messages to the journal with a
custom EVENT_TYPE field and to override journal's default *identifier* (which by default will be ``rsyslogd``):

.. code-block:: shell

   module(load="omjournal")

   template(name="journal" type="list") {
     # Emulate default journal fields
     property(name="msg" outname="MESSAGE")
     property(name="timestamp" outname="SYSLOG_TIMESTAMP")
     property(name="syslogfacility" outname="SYSLOG_FACILITY")
     property(name="syslogseverity" outname="PRIORITY")

     # Custom fields
     constant(value="strange" outname="EVENT_TYPE")
     constant(value="router" outname="SYSLOG_IDENTIFIER")
   }

   action(type="omjournal" template="journal")


Example 2
---------

The `subtree` template is a better fit for structured outputs like this, allowing arbitrary expressions for the destination journal fields using `set` & `reset` directives in *rulsets*.  For instance, here the captured *tags* are translated with `Lookup Tables`
(to facilitae filtering with ``journalctl -t <TAG>``):

.. code-block:: shell

   module(load="omjournal")

   template(name="journal-subtree" type="subtree" subtree="$!")

   lookup_table("tags", ...)

   ruleset(name="journal") {
     # Emulate default journal fields
     set $!MESSAGE = $msg;
     set $!SYSLOG_TIMESTAMP = $timestamp;
     set $!SYSLOG_FACILITY = $syslogfacility;
     set $!PRIORITY = $syslogseverity

     set $!SYSLOG_IDENTIFIER = lookup("tags", $hostname-ip);

     action(type="omjournal" template="journal-subtree")
   }
