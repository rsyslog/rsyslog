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

   Parameter names are case-insensitive; camelCase is recommended for readability.


Action Parameters
-----------------

.. list-table::
   :widths: 30 70
   :header-rows: 1

   * - Parameter
     - Summary
   * - :ref:`param-omjournal-template`
     - .. include:: ../../reference/parameters/omjournal-template.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omjournal-namespace`
     - .. include:: ../../reference/parameters/omjournal-namespace.rst
        :start-after: .. summary-start
        :end-before: .. summary-end

.. toctree::
   :hidden:

   ../../reference/parameters/omjournal-template
   ../../reference/parameters/omjournal-namespace

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


Example 3
---------

The following example shows how to use the namespace feature to filter logs
by facility and write them to different namespaces in the journal. This is useful for journal isolation and classification:

.. code-block:: shell

   module(load="imtcp")
   module(load="omjournal")

   # Each tcp input will trigger the filter ruleset
   input(type="imtcp" port="80" ruleset="output-filter")

   # Filter logs by facility into two different namespaces audit and application
   ruleset(name="output-filter") {
      if ($syslogfacility == 13) then {
         action(type="omjournal" namespace="audit-journal-namespace")
      }
      if ($syslogfacility == 16) then {
         action(type="omjournal" namespace="application-journal-namespace")
      }
   }
   # If you specify a namespace, you must not specify a template. If you do, the action will fail with an error message.
   # namespaces have to be created before use.
