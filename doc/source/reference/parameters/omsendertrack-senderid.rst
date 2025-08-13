.. _param-omsendertrack-senderid:
.. _omsendertrack.parameter.module.senderid:

senderid
========

.. index::
   single: omsendertrack; senderid
   single: senderid

.. summary-start

Template generating the unique key used to track each message sender.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omsendertrack`.

:Name: senderid
:Scope: action
:Type: string (see :doc:`../../rainerscript/constant_strings`)
:Default: action=RSYSLOG_FileFormat
:Required?: no
:Introduced:8.2506.0

Description
-----------
Defines the template that produces a per-sender identifier for maintaining statistics.

Action usage
------------
.. _param-omsendertrack-action-senderid:
.. _omsendertrack.parameter.action.senderid:

.. code-block:: rsyslog

   # Define the template for senderid in omsendertrack
   template(name="id-template" type="list") {
       property(name="hostname")
   }

   action(type="omsendertrack"
          senderid="id-template"
          interval="60"
          statefile="/var/lib/rsyslog/senderstats.json")

See also
--------
See also :doc:`../../configuration/modules/omsendertrack`.

