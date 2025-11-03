.. _param-omsendertrack-senderid:
.. _omsendertrack.parameter.input.senderid:

senderid
========

.. index::
   single: omsendertrack; senderid
   single: senderid

.. summary-start

Sets the template used to derive the unique sender identifier that
omsendertrack tracks.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omsendertrack`.

:Name: senderid
:Scope: input
:Type: string
:Default: input=RSYSLOG_FileFormat
:Required?: no
:Introduced: 8.2506.0 (Proof-of-Concept)

Description
-----------
This parameter defines the **template used to determine the sender's unique
identifier**. The value produced by this template becomes the key for tracking
individual senders within the module's internal statistics.

For instance:

* A simple template like ``"%hostname%"`` tracks each unique host that submits
  messages to rsyslog.
* Using ``"%fromhost-ip%"`` tracks senders based on their IP address.
* A more granular template such as ``"%hostname%-%app-name%"`` differentiates
  between applications on the same host.

**Note:** The processing of this template for every incoming message can impact
overall throughput, especially if complex templates are used. Choose your
template wisely based on your tracking needs and performance considerations.

Input usage
-----------
.. _param-omsendertrack-input-senderid:
.. _omsendertrack.parameter.input.senderid-usage:

.. code-block:: rsyslog

   module(load="omsendertrack")
   action(type="omsendertrack"
          senderId="%hostname%"
          stateFile="/var/lib/rsyslog/senderstats.json")

See also
--------
See also :doc:`../../configuration/modules/omsendertrack`.

