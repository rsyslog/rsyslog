.. _param-omsendertrack-senderid:
.. _omsendertrack.parameter.module.senderid:

senderid
========

.. index::
   single: omsendertrack; senderid
   single: senderid

.. summary-start

Sets the template used to derive the unique sender identifier that omsendertrack tracks.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omsendertrack`.

:Name: senderid
:Scope: module
:Type: string
:Default: module=RSYSLOG_FileFormat
:Required?: no
:Introduced: 8.2506.0 (Proof-of-Concept)

Description
-----------
This parameter defines the **template used to determine the sender's unique identifier**.
The value produced by this template becomes the key for tracking individual senders within the module's internal statistics.

For instance:

* A simple template like ``"%hostname%"`` tracks each unique host that submits messages to rsyslog.
* Using ``"%fromhost-ip%"`` tracks senders based on their IP address.
* A more granular template such as ``"%hostname%-%app-name%"`` differentiates between applications on the same host.

**Note:** The processing of this template for every incoming message can impact overall throughput, especially if complex templates are used.
Choose your template wisely based on your tracking needs and performance considerations.

Module usage
------------
.. _param-omsendertrack-module-senderid:
.. _omsendertrack.parameter.module.senderid-usage:

.. code-block:: rsyslog

   module(load="omsendertrack")
   action(type="omsendertrack"
          senderid="%hostname%"
          statefile="/var/lib/rsyslog/senderstats.json")

See also
--------
See also :doc:`../../configuration/modules/omsendertrack`.

