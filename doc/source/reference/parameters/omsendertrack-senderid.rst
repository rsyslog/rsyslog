.. SPDX-License-Identifier: Apache-2.0

.. meta::
   :description: Configure the omsendertrack senderid action parameter.
   :keywords: rsyslog, omsendertrack, senderid, sender tracking

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
:Scope: action
:Type: string
:Default: sender IP address (``%fromhost-ip%``)
:Required?: no
:Introduced: 8.2506.0 (Proof-of-Concept)

Description
-----------
This parameter names a template used to determine the sender's unique
identifier. Define that template before referencing its name in ``senderId``;
an inline property replacer such as ``senderId="%hostname%"`` is not a template
name. The value produced by the named template becomes the key for tracking
individual senders within the module's internal statistics.

When omitted, the module uses its internal sender-ID template, which expands
``%fromhost-ip%`` and tracks the sender IP address.

For instance:

* A simple template like ``"%hostname%"`` tracks each unique host that submits
  messages to rsyslog.
* Using ``"%fromhost-ip%"`` tracks senders based on their IP address.
* A more granular template such as ``"%hostname%-%app-name%"`` differentiates
  between applications on the same host.

**Note:** The processing of this template for every incoming message can impact
overall throughput, especially if complex templates are used. Choose your
template wisely based on your tracking needs and performance considerations.

Action usage
------------
.. _omsendertrack.parameter.input.senderid-usage:

.. code-block:: rsyslog

   module(load="omsendertrack")
   template(name="sender-hostname" type="string" string="%hostname%")
   action(type="omsendertrack"
          senderId="sender-hostname"
          stateFile="/var/lib/rsyslog/senderstats.json")

See also
--------
See also :doc:`../../configuration/modules/omsendertrack`.

